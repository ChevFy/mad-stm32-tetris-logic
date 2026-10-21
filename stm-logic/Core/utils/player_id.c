#include "player_id.h"

#define PLAYER_ID_PORT GPIOF
#define PLAYER_ID_PIN GPIO_PIN_15
#define PLAYER_ID_SETTLE_MS 2u

static uint8_t	g_player_id = 1;

/*
 * Init pin with pull-up here instead of editing gpio.c,
 * so CubeMX regeneration does not wipe it.
 */
void	player_id_init(void)
{
#if defined(PLAYER_ID_FORCE)
	g_player_id = (PLAYER_ID_FORCE == 2) ? 2 : 1;
#else
	GPIO_InitTypeDef	init = {0};

	init.Pin = PLAYER_ID_PIN;
	init.Mode = GPIO_MODE_INPUT;
	init.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(PLAYER_ID_PORT, &init);
	HAL_Delay(PLAYER_ID_SETTLE_MS);
	if (HAL_GPIO_ReadPin(PLAYER_ID_PORT, PLAYER_ID_PIN) == GPIO_PIN_RESET)
		g_player_id = 2;
	else
		g_player_id = 1;
#endif
}

uint8_t	player_id_get(void)
{
	return (g_player_id);
}
