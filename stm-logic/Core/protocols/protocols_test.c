#include "protocols.h"

#define BOTTOM_ROW 19

static uint8_t	demo_cells[200];

static void	demo_send_frame(protocols_state_t *st)
{
	protocols_pack_board(demo_cells, st->board);
	protocols_send_state(st);
	HAL_Delay(80);
}

static void	demo_drop_to_bottom(protocols_state_t *st, int x, uint8_t color)
{
	for (int y = 0; y <= BOTTOM_ROW; y++)
	{
		if (y > 0)
			demo_cells[(y - 1) * 10 + x] = 0;
		demo_cells[y * 10 + x] = color;
		demo_send_frame(st);
	}
}

void	test_protocols(void)
{
	protocols_state_t	st = {0};

	for (int i = 0; i < 200; i++)
		demo_cells[i] = 0;
	for (int i = 0; i < 4; i++)
	{
		protocols_send_join(1, 0, NULL, 0);
		HAL_Delay(500);
	}
	st.player = 1;
	st.next = 3;
	st.level = 1;
	for (int x = 0; x < 10; x++)
	{
		demo_drop_to_bottom(&st, x, (uint8_t)(1 + (x % 7)));
		st.score += 10;
	}
	for (int flash = 0; flash < 4; flash++)
	{
		for (int x = 0; x < 10; x++)
			demo_cells[BOTTOM_ROW * 10 + x] = (flash % 2) ? 0 : 8;
		demo_send_frame(&st);
	}
	for (int x = 0; x < 10; x++)
		demo_cells[BOTTOM_ROW * 10 + x] = 0;
	st.lines += 1;
	st.score += 100;
	demo_send_frame(&st);
	protocols_send_attack(1);
	HAL_Delay(500);
	protocols_send_gameover();
	HAL_Delay(2000);
}
