/**
 ******************************************************************************
 * @file    player_id.h
 * @brief   Which player this board is (1 or 2), read once at boot.
 *
 *  Same firmware runs on both boards. Player is chosen by a jumper:
 *    PF15 (Zio D2) open          -> player 1  (internal pull-up)
 *    PF15 (Zio D2) shorted to GND -> player 2
 *
 *  Build with -DPLAYER_ID_FORCE=1 or =2 to ignore the jumper.
 *
 *  Usage:
 *    player_id_init();             // once, after MX_GPIO_Init()
 *    uint8_t me = player_id_get(); // 1 or 2
 ******************************************************************************
 */
#ifndef __PLAYER_ID_H__
# define __PLAYER_ID_H__

# ifdef __cplusplus
extern "C"
{
# endif

# include "main.h"

	void player_id_init(void);
	uint8_t player_id_get(void);

# ifdef __cplusplus
}
# endif

#endif /* __PLAYER_ID_H__ */
