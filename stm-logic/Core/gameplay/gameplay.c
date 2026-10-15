#include "gameplay.h"
#include "buttons.h"
#include "protocols.h"
#include <stdbool.h>
#include <string.h>

/* Timing (ms) */
#define WAIT_TIMEOUT_MS 10000u /* auto start 1P if no player 2 */
#define JOIN_RESEND_MS 1000u   /* re-send JOIN so screen does not freeze */
#define FRAME_MS 100u          /* STATE packet period while playing */
#define RESULT_SHOW_MS 5000u   /* how long result screen stays */

typedef struct
{
	game_state_t	state;
	uint32_t		state_since;  /* tick when state was entered */
	uint32_t		last_send;    /* tick of last periodic packet */
	uint8_t			player_count;
	bool			game_over;
	uint8_t			board[BOARD_ROWS][BOARD_COLS];
	uint8_t			next_piece;
	uint8_t			hold_piece;
	uint32_t		score;
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

	st.player = 1;
	protocols_pack_board(&g_game.board[0][0], st.board);
	st.next = g_game.next_piece;
	st.hold = g_game.hold_piece;
	st.score = g_game.score;
	st.level = 1;
	protocols_send_state(&st);
}

static void	start_game(uint8_t players)
{
	g_game.player_count = players;
	g_game.game_over = false;
	memset(g_game.board, 0, sizeof(g_game.board));
	g_game.next_piece = 1;
	g_game.hold_piece = 0;
	g_game.score = 0;
	change_state(GAME_PLAY);
}

/* ---------------------------------------------------------------- states */

static void	on_idle(void)
{
	if (buttons_pressed(BTN_JOIN))
	{
		g_game.player_count = 1;
		protocols_send_join(1, 0, NULL, 0);
		change_state(GAME_WAIT);
	}
}

static void	on_wait(void)
{
	if (buttons_pressed(BTN_JOIN))
	{
		protocols_send_join(2, 0, NULL, 0);
		start_game(2);
	}
	else if (buttons_pressed(BTN_SKIP) || ms_in_state() >= WAIT_TIMEOUT_MS)
	{
		protocols_send_skip_wait();
		start_game(1);
	}
	else if (periodic(JOIN_RESEND_MS))
		protocols_send_join(1, 0, NULL, 0);
}

static void	on_play(void)
{
	/* TODO: real Tetris logic goes here (move/rotate/drop/clear lines). */

	/* Test: BTN_3 or BTN_4 ends the game */
	if (buttons_pressed(BTN_3) || buttons_pressed(BTN_4))
		g_game.game_over = true;

	if (g_game.game_over)
	{
		protocols_send_gameover();
		change_state(GAME_RESULT);
		return ;
	}
	if (periodic(FRAME_MS))
		send_board();
}

static void	on_result(void)
{
	if (ms_in_state() >= RESULT_SHOW_MS)
	{
		g_game.player_count = 0;
		g_game.game_over = false;
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
