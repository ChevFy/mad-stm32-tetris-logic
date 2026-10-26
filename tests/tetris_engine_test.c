#include "tetris_engine.h"
#include "tetris_pieces.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_geometry(void)
{
	for (int p = TETRIS_I; p <= TETRIS_L; p++) {
		for (uint8_t rot = 0; rot < 4; rot++) {
			bool occupied[4][4] = {{false}};
			for (uint8_t i = 0; i < 4; i++) {
				cell_t c = piece_cell((tetris_piece_t)p, rot, i);
				assert(c.r >= 0 && c.r < box_size[p]);
				assert(c.c >= 0 && c.c < box_size[p]);
				assert(!occupied[c.r][c.c]);
				occupied[c.r][c.c] = true;
				cell_t original = piece_cell((tetris_piece_t)p, rot + 4, i);
				assert(c.r == original.r && c.c == original.c);
			}
		}
	}
}

static void test_walls_rotation_and_snapshot(void)
{
	tetris_game_t g;
	tetris_init(&g, 123);
	g.active_piece = TETRIS_O;
	g.active_x = 8;
	tetris_move_right(&g);
	assert(g.active_x == 8);
	g.active_x = 0;
	tetris_move_left(&g);
	assert(g.active_x == 0);
	g.board[0][2] = TETRIS_T;
	tetris_move_right(&g);
	assert(g.active_x == 0); /* Settled block prevents the candidate. */
	memset(g.board, 0, sizeof(g.board));
	g.active_piece = TETRIS_I;
	g.rotation = 1;
	g.active_x = -2; /* Vertical I is in column 0, despite a negative box origin. */
	tetris_rotate_left(&g);
	assert(g.rotation == 1 && g.active_x == -2); /* No wall kick. */

	g.active_piece = TETRIS_T;
	g.rotation = 0;
	g.active_x = 3;
	uint8_t cells[TETRIS_ROWS * TETRIS_COLS];
	tetris_game_t before = g;
	tetris_snapshot(&g, cells);
	assert(memcmp(&g, &before, sizeof(g)) == 0);
	int count = 0;
	for (int i = 0; i < TETRIS_ROWS * TETRIS_COLS; i++)
		count += cells[i] != 0;
	assert(count == 4);
}

static void test_line_clears_and_levels(void)
{
	const uint16_t points[] = {0, 40, 100, 300, 1200};
	for (int n = 1; n <= 4; n++) {
		tetris_game_t g;
		tetris_init(&g, 42);
		g.lines = 9;
		for (int row = TETRIS_ROWS - n; row < TETRIS_ROWS; row++)
			for (int col = 0; col < TETRIS_COLS; col++)
				g.board[row][col] = col == 4 ? 0 : TETRIS_J;
		g.active_piece = TETRIS_I;
		g.rotation = 1;
		g.active_x = 2;
		g.active_y = TETRIS_ROWS - 4;
		tetris_gravity_step(&g); /* Floor blocks descent: lock, clear, score, spawn. */
		assert(!g.game_over);
		assert(g.lines == 9 + n && g.level == 2);
		assert(g.score == points[n]); /* Uses level before the clear. */
		int remaining = 0;
		for (int r = 0; r < TETRIS_ROWS; r++)
			for (int c = 0; c < TETRIS_COLS; c++)
				remaining += g.board[r][c] != 0;
		assert(remaining == 4 - n);
	}
}

static void test_hold_and_spawn_gameover(void)
{
	tetris_game_t g;
	tetris_init(&g, 456);
	tetris_piece_t first = g.active_piece;
	tetris_piece_t next = g.next_piece;
	tetris_hold(&g);
	assert(g.held_piece == first && g.active_piece == next && g.hold_used);
	tetris_game_t before = g;
	tetris_hold(&g);
	assert(memcmp(&g, &before, sizeof(g)) == 0);
	/* Lock the incoming piece and verify that holding is enabled again. */
	while (g.hold_used && !g.game_over)
		tetris_gravity_step(&g);
	assert(!g.game_over && !g.hold_used);
	tetris_piece_t outgoing = g.active_piece;
	next = g.next_piece;
	uint32_t rng = g.rng_state;
	tetris_hold(&g);
	assert(g.active_piece == first && g.held_piece == outgoing && g.hold_used);
	assert(g.next_piece == next && g.rng_state == rng && g.rotation == 0);

	tetris_init(&g, 789);
	memset(g.board, TETRIS_J, sizeof(g.board));
	tetris_hold(&g);
	assert(g.game_over);
	before = g;
	tetris_move_left(&g);
	tetris_move_right(&g);
	tetris_rotate_left(&g);
	tetris_hold(&g);
	tetris_gravity_step(&g);
	assert(memcmp(&g, &before, sizeof(g)) == 0);
}

static void test_rng_and_speed(void)
{
	tetris_game_t a, b;
	tetris_init(&a, 0);
	tetris_init(&b, 0);
	bool repeated = false;
	tetris_piece_t previous = TETRIS_EMPTY;
	for (int i = 0; i < 1000; i++) {
		tetris_piece_t p = tetris_rng_piece(&a);
		assert(p >= TETRIS_I && p <= TETRIS_L);
		assert(p == tetris_rng_piece(&b));
		repeated |= p == previous;
		previous = p;
	}
	assert(repeated);
	for (uint8_t level = 1; level <= 30; level++) {
		assert(tetris_gravity_interval_ms(level) > 0);
		if (level > 1)
			assert(tetris_gravity_interval_ms(level)
				<= tetris_gravity_interval_ms(level - 1));
	}
	assert(tetris_gravity_interval_ms(255) == tetris_gravity_interval_ms(30));
}

int main(void)
{
	test_geometry();
	test_walls_rotation_and_snapshot();
	test_line_clears_and_levels();
	test_hold_and_spawn_gameover();
	test_rng_and_speed();
	puts("Tetris engine checks passed.");
	return 0;
}
