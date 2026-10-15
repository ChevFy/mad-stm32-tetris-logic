/**
 ******************************************************************************
 * @file    buttons.h
 * @brief   4 push buttons (active-low, internal pull-up) with edge detection.
 *
 *  Usage:
 *    buttons_init();               // once, after MX_GPIO_Init()
 *    buttons_update();             // once per loop
 *    if (buttons_pressed(BTN_JOIN)) // true only on the loop it was pressed
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
		BTN_JOIN = 0, /* PF13 red   : join / add player 2 */
		BTN_SKIP,     /* PF14 green : skip waiting for player 2 */
		BTN_3,        /* PE9        : (test) end game */
		BTN_4,        /* PE11       : (test) end game */
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
