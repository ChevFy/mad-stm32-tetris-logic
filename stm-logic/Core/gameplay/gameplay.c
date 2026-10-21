#include "gameplay.h"
#include "buttons.h"
#include "protocols.h"
#include "tetris_engine.h"
#include "usart.h"      /* huart2 = ESP32 pay link */
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/* Timing (ms) */
#define WAIT_TIMEOUT_MS 10000u /* automatically start solo after lobby wait */
#define JOIN_RESEND_MS 1000u   /* re-send JOIN so screen does not freeze */
#define FRAME_MS 100u          /* STATE packet period while playing */
#define RESULT_SHOW_MS 5000u   /* how long result screen stays */
#define SOFT_DROP_MS 40u       /* gravity interval while SOFT_DROP is held */

/* TEMP diagnostic: press START -> send a test CREDIT to the Hub (bypass pay),
 * to prove the LOGIC->HUB USART6 link alone. Set 0 for normal pay-gated play. */
#define PAY_SELFTEST 0

typedef struct
{
	game_state_t	state;
	uint32_t		state_since;  /* tick when state was entered */
	uint32_t		last_send;    /* tick of last periodic packet */
	uint8_t			player_count;
	tetris_game_t	tetris;
	uint32_t 		last_gravity_ms;   /* when gravity last fired */
}	game_t;

static game_t	g_game;

/* ----------------------------------------------------------------- pay link
 * ESP32 taps an RFID card, pays over MQTT, then sends one line to USART2:
 *   "PAY OK <balance>\n"    -> grant one game credit, relay CREDIT_OK to Hub
 *   "PAY DENY <balance>\n"  -> no credit, relay CREDIT_DENIED (red overlay)
 */
static struct
{
	int		credits;      /* unspent paid games (gates START) */
	int32_t	balance;      /* last reported account balance */
	char	name[17];     /* member username ("" = guest, no name) */
	char	line[48];
	uint8_t	len;
}	g_pay;

/* USART2 RX ring, filled by the interrupt (polling + HAL_Delay dropped bytes). */
#define RX2_SIZE 256
static volatile uint8_t		g_rx2[RX2_SIZE];
static volatile uint16_t	g_rx2_head, g_rx2_tail;

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

static void	pay_init(void)
{
	GPIO_InitTypeDef	io = {0};

	__HAL_RCC_GPIOB_CLK_ENABLE();
	io.Pin  = GPIO_PIN_0;           /* Nucleo-144 LD1 (green) */
	io.Mode = GPIO_MODE_OUTPUT_PP;
	HAL_GPIO_Init(GPIOB, &io);

	/* Enable USART2 RX interrupt (MX_USART2_UART_Init already set up pins). */
	USART2->CR1 |= USART_CR1_RXNEIE;
	HAL_NVIC_SetPriority(USART2_IRQn, 7, 0);
	HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void	pay_parse_line(void)
{
	char	*p;
	char	*end;

	/* Any recognised line -> latch LD1 on: proves USART2 RX from ESP32. */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

	if (strncmp(g_pay.line, "PAY OK", 6) == 0)
	{
		/* "PAY OK <balance> <name...>"  (name empty for a guest) */
		g_pay.balance = (int32_t)strtol(g_pay.line + 6, &end, 10);
		while (*end == ' ')
			end++;
		strncpy(g_pay.name, end, 16);
		g_pay.name[16] = '\0';
		g_pay.credits = 1;
		protocols_send_credit(PROTO_CREDIT_OK, g_pay.balance);
	}
	else if (strncmp(g_pay.line, "PAY NOCARD", 10) == 0)
	{
		g_pay.name[0] = '\0';
		protocols_send_credit(PROTO_CREDIT_NO_CARD, 0);
	}
	else if (strncmp(g_pay.line, "PAY DENY", 8) == 0)
	{
		g_pay.name[0] = '\0';
		g_pay.balance = (int32_t)atol(g_pay.line + 8);
		protocols_send_credit(PROTO_CREDIT_DENIED, g_pay.balance);
	}
	else if (strncmp(g_pay.line, "LB ", 3) == 0)
	{
		/* "LB <rank> <count> <points> <name...>" -> one leaderboard row */
		long	rank = strtol(g_pay.line + 3, &end, 10);
		long	count = strtol(end, &end, 10);
		long	points = strtol(end, &end, 10);
		while (*end == ' ')
			end++;
		p = end;
		protocols_send_leaderboard((uint8_t)rank, (uint8_t)count,
			(uint32_t)points, p);
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

/* ---------------------------------------------------------------- helpers */

static void	change_state(game_state_t next)
{
	g_game.state = next;
	g_game.state_since = HAL_GetTick();
	g_game.last_send = g_game.state_since;
}

static uint32_t	ms_in_state(void)
{
	return (HAL_GetTick() - g_game.state_since);
}

static bool	periodic(uint32_t period_ms)
{
	uint32_t	now;

	now = HAL_GetTick();
	if (now - g_game.last_send < period_ms)
		return (false);
	g_game.last_send = now;
	return (true);
}

static void	send_board(void)
{
	protocols_state_t	st = {0};
	uint8_t				cells[TETRIS_ROWS * TETRIS_COLS];

	st.player = 1;
	tetris_snapshot(&g_game.tetris, cells);
	protocols_pack_board(cells, st.board);
	st.next = (uint8_t)g_game.tetris.next_piece;
	st.hold = (uint8_t)g_game.tetris.held_piece;
	st.score = g_game.tetris.score;
	st.lines = g_game.tetris.lines;
	st.level = g_game.tetris.level;
	protocols_send_state(&st);
}

static void	start_game(uint8_t players)
{
	g_game.player_count = players;
	tetris_init(&g_game.tetris, HAL_GetTick());
	uint32_t now = HAL_GetTick();
	g_game.last_gravity_ms = now;
	change_state(GAME_PLAY);
}

/* ---------------------------------------------------------------- states */

static void	on_idle(void)
{
#if PAY_SELFTEST
	if (buttons_pressed(BTN_START))
		protocols_send_credit(PROTO_CREDIT_OK, 99);  /* prove USART6 -> Hub */
#endif
	/* Gate: only start once a card has paid (credit > 0). */
	if (buttons_pressed(BTN_START) && g_pay.credits > 0)
	{
		const char	*nm = (g_pay.name[0] != '\0') ? g_pay.name : NULL;

		g_pay.credits--;
		g_game.player_count = 1;
		protocols_send_join(1, 0, nm, 0);   /* member name shown in lobby */
		change_state(GAME_WAIT);
	}
}

static void	on_wait(void)
{
	if (buttons_pressed(BTN_HOLD) || ms_in_state() >= WAIT_TIMEOUT_MS)
	{
		protocols_send_skip_wait();
		start_game(1);
	}
	else if (periodic(JOIN_RESEND_MS))
		protocols_send_join(1, 0, NULL, 0);
}

static void	on_play(void)
{
	tetris_game_t *t = &g_game.tetris;
	uint32_t now = HAL_GetTick();

	if (t->game_over)
	{
		send_board(); /* Publish the final settled board/stats before GAMEOVER. */
		protocols_send_gameover();
		change_state(GAME_RESULT);
		return ;
	}

	if (buttons_pressed(BTN_LEFT))
		tetris_move_left(t);
	if (buttons_pressed(BTN_RIGHT))
		tetris_move_right(t);
	if (buttons_pressed(BTN_ROTATE_LEFT))
		tetris_rotate_left(t);
	if (buttons_pressed(BTN_HOLD))
		tetris_hold(t);

	uint32_t gravity_ms = tetris_gravity_interval_ms(t->level);
	if (buttons_held(BTN_SOFT_DROP) && SOFT_DROP_MS < gravity_ms)
		gravity_ms = SOFT_DROP_MS;   /* hold to fall faster */
	if (now - g_game.last_gravity_ms >= gravity_ms) {
		tetris_gravity_step(t);
		g_game.last_gravity_ms = now;
	}

	if (periodic(FRAME_MS))
		send_board();
}

static void	on_result(void)
{
	if (ms_in_state() >= RESULT_SHOW_MS)
	{
		g_game.player_count = 0;
		change_state(GAME_IDLE);
	}
}

/* ---------------------------------------------------------------- public */

void	gameplay_init(void)
{
	memset(&g_game, 0, sizeof(g_game));
	buttons_init();
	pay_init();
	change_state(GAME_IDLE);
}

void	gameplay_update(void)
{
	buttons_update();
	pay_poll();
	switch (g_game.state)
	{
	case GAME_IDLE:
		on_idle();
		break ;
	case GAME_WAIT:
		on_wait();
		break ;
	case GAME_PLAY:
		on_play();
		break ;
	case GAME_RESULT:
		on_result();
		break ;
	}
}

game_state_t	gameplay_state(void)
{
	return (g_game.state);
}
