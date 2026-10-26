#include "buttons.h"

#define DEBOUNCE_MS 30u

typedef struct
{
	GPIO_TypeDef	*port;
	uint16_t		pin;
	bool			active_low;
}	button_pin_t;

static const button_pin_t	g_pins[BTN_COUNT] = {
	[BTN_LEFT] = {GPIOF, GPIO_PIN_13, true},
	[BTN_HOLD] = {GPIOF, GPIO_PIN_14, true},
	[BTN_RIGHT] = {GPIOE, GPIO_PIN_9, true},
	[BTN_ROTATE_LEFT] = {GPIOE, GPIO_PIN_11, true},
	[BTN_SOFT_DROP] = {GPIOF, GPIO_PIN_12, true},
	[BTN_START] = {GPIOC, GPIO_PIN_13, false},
	[BTN2_LEFT] = {GPIOE, GPIO_PIN_10, true},
	[BTN2_RIGHT] = {GPIOE, GPIO_PIN_12, true},
	[BTN2_ROTATE_LEFT] = {GPIOE, GPIO_PIN_13, true},
	[BTN2_HOLD] = {GPIOE, GPIO_PIN_14, true},
	[BTN2_SOFT_DROP] = {GPIOE, GPIO_PIN_15, true},
};

static bool		g_held[BTN_COUNT];
static bool		g_pressed[BTN_COUNT];
static uint32_t	g_last_change[BTN_COUNT];

/*
 * Apply active-low control pull-ups and the active-high USER pull-down here,
 * so CubeMX regeneration does not wipe it.
 */
void	buttons_init(void)
{
	GPIO_InitTypeDef	init = {0};

	__HAL_RCC_GPIOC_CLK_ENABLE();
	init.Mode = GPIO_MODE_INPUT;
	for (int i = 0; i < BTN_COUNT; i++)
	{
		init.Pin = g_pins[i].pin;
		init.Pull = g_pins[i].active_low ? GPIO_PULLUP : GPIO_PULLDOWN;
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
		if (g_pins[i].active_low)
			down = (HAL_GPIO_ReadPin(g_pins[i].port, g_pins[i].pin) == GPIO_PIN_RESET);
		else
			down = (HAL_GPIO_ReadPin(g_pins[i].port, g_pins[i].pin) == GPIO_PIN_SET);
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
