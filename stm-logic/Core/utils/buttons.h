/**
 ******************************************************************************
 * @file    buttons.h
 * @brief   Tetris controls and start button with polling/debounce.
 *
 *  Usage:
 *    buttons_init();               // once, after MX_GPIO_Init()
 *    buttons_update();             // once per loop
 *    if (buttons_pressed(BTN_START)) // true only on the loop it was pressed
 ******************************************************************************
 */
#ifndef __BUTTONS_H__
# define __BUTTONS_H__

# ifdef __cplusplus
extern "C"
{
# endif

# include "main.h"
# include <stdbool.h>

	typedef enum
	{
		BTN_LEFT = 0,       /* PF13: move left */
		BTN_HOLD,            /* PF14: hold in game; skip wait in lobby */
		BTN_RIGHT,           /* PE9: move right */
		BTN_ROTATE_LEFT,     /* PE11: rotate left */
		BTN_START,           /* PC13: Nucleo USER button */
		BTN_COUNT
	}	button_id_t;

	void buttons_init(void);
	void buttons_update(void);
	bool buttons_pressed(button_id_t id);
	bool buttons_held(button_id_t id);

# ifdef __cplusplus
}
# endif

#endif /* __BUTTONS_H__ */
