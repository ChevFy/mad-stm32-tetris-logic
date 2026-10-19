#include "gameplay.h"
#include "buttons.h"
#include "protocols.h"
#include "tetris_engine.h"
#include <stdbool.h>
#include <string.h>

/* Timing (ms) */
#define WAIT_TIMEOUT_MS 10000u /* automatically start solo after lobby wait */
#define JOIN_RESEND_MS 1000u   /* re-send JOIN so screen does not freeze */
#define FRAME_MS 100u          /* STATE packet period while playing */
#define RESULT_SHOW_MS 5000u   /* how long result screen stays */

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
	if (buttons_pressed(BTN_START))
	{
		g_game.player_count = 1;
		protocols_send_join(1, 0, NULL, 0);
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

	if (now - g_game.last_gravity_ms >= tetris_gravity_interval_ms(t->level)) {
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
	change_state(GAME_IDLE);
}

void	gameplay_update(void)
{
	buttons_update();
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
