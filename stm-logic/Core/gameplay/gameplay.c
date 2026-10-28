#include "gameplay.h"
#include "buttons.h"
#include "protocols.h"
#include "tetris_engine.h"
#include "usart.h"      /* huart2 = ESP32 pay link */
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Timing (ms) */
#define WAIT_TIMEOUT_MS 10000u  /* no 2nd player within this -> start solo */
#define JOIN_RESEND_MS 1000u    /* resend JOIN so the Hub keeps the side alive */
#define FRAME_MS 100u           /* STATE packet period while playing */
#define RESULT_SHOW_MS 5000u    /* result screen duration */
#define SOFT_DROP_MS 40u        /* gravity interval while soft drop held */

/* ----------------------------------------------------------------- pay link
 * ESP32 sends ASCII lines on USART2 (interrupt -> ring):
 *   "PAY OK <balance> <spent> <name>"  (name empty = guest) -> a paid join
 *   "PAY DENY <balance>" / "PAY NOCARD"                      -> show + ignore
 *   "LB <rank> <count> <points> <name>"                     -> relay to Hub
 *   "NEWREC <points>"                                        -> relay to Hub
 */
#define RX2_SIZE 256
static volatile uint8_t  g_rx2[RX2_SIZE];
static volatile uint16_t g_rx2_head, g_rx2_tail;

void	USART2_IRQHandler(void)
{
	uint32_t	isr = USART2->ISR;

	if (isr & USART_ISR_ORE)
		USART2->ICR = USART_ICR_ORECF;
	if (isr & USART_ISR_RXNE)
	{
		uint8_t		b = (uint8_t)USART2->RDR;
		uint16_t	next = (uint16_t)((g_rx2_head + 1) % RX2_SIZE);
		if (next != g_rx2_tail)
		{
			g_rx2[g_rx2_head] = b;
			g_rx2_head = next;
		}
	}
}

/* A paid tap waiting to be turned into a player join. */
static struct
{
	bool	pending;
	bool	is_member;
	char	name[17];
	char	line[48];
	uint8_t	len;
}	g_pay;

static void	pay_parse_line(void)
{
	char	*end;

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);  /* LD1: RX proof */

	if (strncmp(g_pay.line, "PAY OK", 6) == 0)
	{
		int32_t	balance = (int32_t)strtol(g_pay.line + 6, &end, 10);
		int32_t	spent = (int32_t)strtol(end, &end, 10);

		while (*end == ' ')
			end++;
		strncpy(g_pay.name, end, 16);
		g_pay.name[16] = '\0';
		g_pay.is_member = (g_pay.name[0] != '\0');  /* name present = member */
		g_pay.pending = true;
		protocols_send_credit(PROTO_CREDIT_OK, balance, spent);
	}
	else if (strncmp(g_pay.line, "PAY NOCARD", 10) == 0)
	{
		protocols_send_credit(PROTO_CREDIT_NO_CARD, 0, 0);
	}
	else if (strncmp(g_pay.line, "PAY DENY", 8) == 0)
	{
		protocols_send_credit(PROTO_CREDIT_DENIED,
			(int32_t)atol(g_pay.line + 8), 0);
	}
	else if (strncmp(g_pay.line, "NEWREC ", 7) == 0)
	{
		protocols_send_newrecord((uint32_t)strtoul(g_pay.line + 7, NULL, 10));
	}
	else if (strncmp(g_pay.line, "LB ", 3) == 0)
	{
		long	rank = strtol(g_pay.line + 3, &end, 10);
		long	count = strtol(end, &end, 10);
		long	points = strtol(end, &end, 10);

		while (*end == ' ')
			end++;
		protocols_send_leaderboard((uint8_t)rank, (uint8_t)count,
			(uint32_t)points, end);
	}
}

static void	pay_poll(void)
{
	while (g_rx2_tail != g_rx2_head)
	{
		uint8_t	b = g_rx2[g_rx2_tail];
		g_rx2_tail = (uint16_t)((g_rx2_tail + 1) % RX2_SIZE);
		if (b == '\n' || b == '\r')
		{
			if (g_pay.len > 0)
			{
				g_pay.line[g_pay.len] = '\0';
				pay_parse_line();
				g_pay.len = 0;
			}
		}
		else if (g_pay.len < sizeof(g_pay.line) - 1)
			g_pay.line[g_pay.len++] = (char)b;
	}
}

/* Report the final score to the ESP32 (saved only for 1P members). Sending 0
 * in 2P just releases the ESP32 play-lock without saving. */
static void	pay_report_score(uint32_t points)
{
	char	buf[24];
	int		n = snprintf(buf, sizeof(buf), "SCORE %lu\n", (unsigned long)points);

	HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)n, 100);
}

/* Tell the ESP32 whether taps are accepted (only in lobby / join-wait). */
static void	pay_set_open(int open)
{
	const char	*msg = open ? "OPEN 1\n" : "OPEN 0\n";

	HAL_UART_Transmit(&huart2, (uint8_t *)msg, 7, 50);
}

static void	pay_init(void)
{
	GPIO_InitTypeDef	io = {0};

	__HAL_RCC_GPIOB_CLK_ENABLE();
	io.Pin  = GPIO_PIN_0;           /* Nucleo-144 LD1 (green) = RX proof */
	io.Mode = GPIO_MODE_OUTPUT_PP;
	HAL_GPIO_Init(GPIOB, &io);

	USART2->CR1 |= USART_CR1_RXNEIE;
	HAL_NVIC_SetPriority(USART2_IRQn, 7, 0);
	HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* -------------------------------------------------------------------- game */
typedef struct
{
	tetris_game_t	t;
	bool			active;
	bool			member;
	char			name[17];
	uint32_t		last_gravity;
}	player_t;

static struct
{
	game_state_t	state;
	uint32_t		since;
	uint32_t		last_send;
	int				count;      /* 1 or 2 active players */
	player_t		p[2];       /* [0] = player 1, [1] = player 2 */
	int				winner;     /* 2P: 1 or 2; 0 = none/solo */
}	g;

/* per-player button ids */
static const button_id_t	BTN_L[2]   = { BTN_LEFT, BTN2_LEFT };
static const button_id_t	BTN_R[2]   = { BTN_RIGHT, BTN2_RIGHT };
static const button_id_t	BTN_ROT[2] = { BTN_ROTATE_LEFT, BTN2_ROTATE_LEFT };
static const button_id_t	BTN_HLD[2] = { BTN_HOLD, BTN2_HOLD };
static const button_id_t	BTN_SD[2]  = { BTN_SOFT_DROP, BTN2_SOFT_DROP };

/* Standard versus garbage: single=0, double=1, triple=2, tetris=4. */
static uint8_t	garbage_for(int cleared)
{
	static const uint8_t g_tbl[5] = { 0, 0, 1, 2, 4 };
	if (cleared < 0 || cleared > 4)
		return 0;
	return g_tbl[cleared];
}

static void	change_state(game_state_t next)
{
	g.state = next;
	g.since = HAL_GetTick();
	g.last_send = g.since;
	pay_set_open(next == GAME_IDLE || next == GAME_WAIT);
}

static uint32_t	ms_in_state(void) { return HAL_GetTick() - g.since; }

static bool	periodic(uint32_t period_ms)
{
	uint32_t	now = HAL_GetTick();

	if (now - g.last_send < period_ms)
		return false;
	g.last_send = now;
	return true;
}

static void	send_board(int i)
{
	protocols_state_t	st = {0};
	uint8_t				cells[TETRIS_ROWS * TETRIS_COLS];

	st.player = (uint8_t)(i + 1);
	tetris_snapshot(&g.p[i].t, cells);
	protocols_pack_board(cells, st.board);
	st.next = (uint8_t)g.p[i].t.next_piece;
	st.hold = (uint8_t)g.p[i].t.held_piece;
	st.score = g.p[i].t.score;
	st.lines = g.p[i].t.lines;
	st.level = g.p[i].t.level;
	st.garbage_wait = g.p[i].t.pending_garbage;
	protocols_send_state(&st);
}

static void	join_player(int i, const char *name, bool member)
{
	g.p[i].active = true;
	g.p[i].member = member;
	strncpy(g.p[i].name, name ? name : "", sizeof(g.p[i].name) - 1);
	g.p[i].name[sizeof(g.p[i].name) - 1] = '\0';
	protocols_send_join((uint8_t)(i + 1), 0,
		g.p[i].name[0] ? g.p[i].name : NULL, 0);
}

static void	start_game(void)
{
	uint32_t	now = HAL_GetTick();
	uint32_t	seed = now;   /* same seed both players -> fair piece sequence */

	g.winner = 0;
	for (int i = 0; i < g.count; i++)
	{
		tetris_init(&g.p[i].t, seed);
		g.p[i].last_gravity = now;
	}
	change_state(GAME_PLAY);
}

/* ---------------------------------------------------------------- states */

static void	on_idle(void)
{
	if (g_pay.pending)            /* first paid tap -> player 1 */
	{
		g_pay.pending = false;
		g.count = 1;
		join_player(0, g_pay.name, g_pay.is_member);
		change_state(GAME_WAIT);
	}
}

static void	on_wait(void)
{
	if (g_pay.pending)           /* second paid tap -> 2 players, start now */
	{
		g_pay.pending = false;
		g.count = 2;
		join_player(1, g_pay.name, g_pay.is_member);
		start_game();
	}
	else if (ms_in_state() >= WAIT_TIMEOUT_MS)
	{
		protocols_send_skip_wait();
		start_game();            /* solo (count stays 1) */
	}
	else if (periodic(JOIN_RESEND_MS))
	{
		protocols_send_join(1, 0, g.p[0].name[0] ? g.p[0].name : NULL, 0);
	}
}

static void	step_player(int i, uint32_t now)
{
	player_t	*pl = &g.p[i];

	if (pl->t.game_over)
		return;
	if (buttons_pressed(BTN_L[i]))   tetris_move_left(&pl->t);
	if (buttons_pressed(BTN_R[i]))   tetris_move_right(&pl->t);
	if (buttons_pressed(BTN_ROT[i])) tetris_rotate_left(&pl->t);
	if (buttons_pressed(BTN_HLD[i])) tetris_hold(&pl->t);

	uint32_t	gms = tetris_gravity_interval_ms(pl->t.level);
	if (buttons_held(BTN_SD[i]) && SOFT_DROP_MS < gms)
		gms = SOFT_DROP_MS;
	if (now - pl->last_gravity >= gms)
	{
		uint16_t	before = pl->t.lines;
		tetris_gravity_step(&pl->t);
		pl->last_gravity = now;
		if (g.count == 2)
		{
			int	cleared = (int)(pl->t.lines - before);
			uint8_t	rows = garbage_for(cleared);
			/* Cancel incoming garbage before attacking the opponent. */
			uint8_t	cancelled = (rows < pl->t.pending_garbage)
				? rows : pl->t.pending_garbage;
			pl->t.pending_garbage -= cancelled;
			rows -= cancelled;
			if (rows)
				tetris_queue_garbage(&g.p[1 - i].t, rows);  /* attack opponent */
		}
	}
}

static void	end_solo(void)
{
	send_board(0);
	protocols_send_gameover();
	pay_report_score(g.p[0].member ? g.p[0].t.score : 0);  /* save if member */
	change_state(GAME_RESULT);
}

static void	end_versus(void)
{
	int	w = g.winner;
	int	l = (w == 1) ? 2 : 1;

	/* send loser's STATE last so the Hub marks the loser correctly */
	send_board(w - 1);
	send_board(l - 1);
	protocols_send_gameover();
	pay_report_score(0);   /* no leaderboard save in 2P; just free ESP32 lock */
	change_state(GAME_RESULT);
}

static void	on_play(void)
{
	uint32_t	now = HAL_GetTick();

	for (int i = 0; i < g.count; i++)
		step_player(i, now);

	if (g.count == 1)
	{
		if (g.p[0].t.game_over)
			end_solo();
		else if (periodic(FRAME_MS))
			send_board(0);
		return;
	}

	/* 2 players */
	if (g.p[0].t.game_over || g.p[1].t.game_over)
	{
		if (g.p[0].t.game_over && !g.p[1].t.game_over)
			g.winner = 2;
		else if (g.p[1].t.game_over && !g.p[0].t.game_over)
			g.winner = 1;
		else
			g.winner = 1;   /* both topped same tick: pick P1 */
		end_versus();
		return;
	}
	if (periodic(FRAME_MS))
	{
		send_board(0);
		send_board(1);
	}
}

static void	on_result(void)
{
	if (ms_in_state() >= RESULT_SHOW_MS)
	{
		memset(&g.p, 0, sizeof(g.p));
		g.count = 0;
		g.winner = 0;
		change_state(GAME_IDLE);
	}
}

/* ---------------------------------------------------------------- public */

void	gameplay_init(void)
{
	memset(&g, 0, sizeof(g));
	buttons_init();
	pay_init();
	change_state(GAME_IDLE);
}

void	gameplay_update(void)
{
	static uint32_t	last_open = 0;
	uint32_t		now = HAL_GetTick();

	buttons_update();
	pay_poll();
	if (now - last_open >= 500)   /* resend accept-state so ESP32 can't miss it */
	{
		pay_set_open(g.state == GAME_IDLE || g.state == GAME_WAIT);
		last_open = now;
	}
	switch (g.state)
	{
	case GAME_IDLE:   on_idle();   break;
	case GAME_WAIT:   on_wait();   break;
	case GAME_PLAY:   on_play();   break;
	case GAME_RESULT: on_result(); break;
	}
}

game_state_t	gameplay_state(void) { return g.state; }
