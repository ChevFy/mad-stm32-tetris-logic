/**
 ******************************************************************************
 * @file    gameplay.h
 * @brief   Game flow state machine (lobby -> play -> result).
 *
 *  Flow:
 *    IDLE    --START-->                    WAIT   (send JOIN p1)
 *    WAIT    --HOLD or 10 s timeout-->     PLAY   (send SKIP_WAIT, 1 player)
 *    PLAY    --game over-->                RESULT (send GAMEOVER)
 *    RESULT  --5 s-->                      IDLE
 *
 *  Usage (main.c):
 *    gameplay_init();
 *    while (1) { gameplay_update(); HAL_Delay(10); }
 ******************************************************************************
 */
#ifndef __GAMEPLAY_H__
# define __GAMEPLAY_H__

# ifdef __cplusplus
extern "C"
{
# endif

# include "main.h"

# define BOARD_ROWS 20
# define BOARD_COLS 10

	typedef enum
	{
		GAME_IDLE,   /* waiting for first player */
		GAME_WAIT,   /* joined; waiting for solo timeout or skip */
		GAME_PLAY,   /* game running */
		GAME_RESULT  /* game over, showing score */
	}	game_state_t;

	void gameplay_init(void);
	void gameplay_update(void);
	game_state_t gameplay_state(void);

# ifdef __cplusplus
}
# endif

#endif /* __GAMEPLAY_H__ */
