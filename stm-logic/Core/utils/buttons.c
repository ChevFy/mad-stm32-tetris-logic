#include "buttons.h"

#define DEBOUNCE_MS 30u

typedef struct
{
	GPIO_TypeDef	*port;
	uint16_t		pin;
}	button_pin_t;

static const button_pin_t	g_pins[BTN_COUNT] = {
	[BTN_JOIN] = {GPIOF, GPIO_PIN_13},
	[BTN_SKIP] = {GPIOF, GPIO_PIN_14},
	[BTN_3] = {GPIOE, GPIO_PIN_9},
	[BTN_4] = {GPIOE, GPIO_PIN_11},
};

static bool		g_held[BTN_COUNT];
static bool		g_pressed[BTN_COUNT];
static uint32_t	g_last_change[BTN_COUNT];

/*
 * Re-init pins with pull-up here instead of editing gpio.c,
 * so CubeMX regeneration does not wipe it.
 */
void	buttons_init(void)
{
	GPIO_InitTypeDef	init = {0};

	init.Mode = GPIO_MODE_INPUT;
	init.Pull = GPIO_PULLUP;
	for (int i = 0; i < BTN_COUNT; i++)
	{
		init.Pin = g_pins[i].pin;
		HAL_GPIO_Init(g_pins[i].port, &init);
		g_held[i] = false;
		g_pressed[i] = false;
		g_last_change[i] = 0;
	}
}

void	buttons_update(void)
{
	uint32_t	now;
	bool		down;

	now = HAL_GetTick();
	for (int i = 0; i < BTN_COUNT; i++)
	{
		g_pressed[i] = false;
		down = (HAL_GPIO_ReadPin(g_pins[i].port, g_pins[i].pin) == GPIO_PIN_RESET);
		if (down == g_held[i] || now - g_last_change[i] < DEBOUNCE_MS)
			continue ;
		g_held[i] = down;
		g_last_change[i] = now;
		if (down)
			g_pressed[i] = true;
	}
}

bool	buttons_pressed(button_id_t id)
{
	return (id < BTN_COUNT && g_pressed[id]);
}

bool	buttons_held(button_id_t id)
{
	return (id < BTN_COUNT && g_held[id]);
}
