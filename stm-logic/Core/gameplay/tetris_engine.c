#include "tetris_engine.h"
#include "tetris_pieces.h"
#include <string.h>

static const uint16_t scores[5] = {0, 40, 100, 300, 1200};

void spawn(tetris_game_t *game, tetris_piece_t p) {
	game->active_piece = p;
	game->rotation = 0;
	game->active_x = (TETRIS_COLS - box_size[p]) / 2;
	game->active_y = 0;
	if (!tetris_can_place(game, p, 0, game->active_x, game->active_y)) {
		game->game_over = true;
	}
}

void spawn_from_queue(tetris_game_t *game) {
	spawn(game, game->next_piece);
	game->next_piece = tetris_rng_piece(game);
}

void tetris_init(tetris_game_t *game, uint32_t seed)
{
	if (game == NULL)
		return ;
	memset(game, 0, sizeof(*game));
	game->rng_state = seed == 0 ? 0xA341316Cu : seed;
	game->level = 1;
	game->next_piece = tetris_rng_piece(game);
	spawn(game, tetris_rng_piece(game));
}

bool tetris_can_place(const tetris_game_t *game, tetris_piece_t piece,
		uint8_t rotation, int8_t x, int8_t y)
{
	for (uint8_t i = 0; i < 4; i++) {
		cell_t c = piece_cell(piece, rotation, i);
		int bx = x + c.c, by = y + c.r;
		if (bx < 0 || bx >= TETRIS_COLS || by >= TETRIS_ROWS) return false;
		if (by >= 0 && game->board[by][bx] != 0) return false;
	}
	return true;
}

void tetris_move_left(tetris_game_t *game)
{
	if (game->game_over) return;
	if (tetris_can_place(game, game->active_piece, game->rotation, game->active_x - 1, game->active_y)) {
		game->active_x--;
	}
}

void tetris_move_right(tetris_game_t *game)
{
	if (game->game_over) return;
	if (tetris_can_place(game, game->active_piece, game->rotation, game->active_x + 1, game->active_y)) {
		game->active_x++;
	}
}

void tetris_rotate_left(tetris_game_t *game)
{
	if (game->game_over || game->active_piece == TETRIS_O) return;
	uint8_t nr = (game->rotation + 3) & 3;
	if (tetris_can_place(game, game->active_piece, nr, game->active_x, game->active_y)) {
		game->rotation = nr;
	}
}

int clear_lines(tetris_game_t *game) {
	int cleared = 0;
	for (int r = TETRIS_ROWS - 1; r >= 0;) {
		bool full = true;
		for (int c = 0; c < TETRIS_COLS; c++)
			if (!game->board[r][c]) { full = false; break; }
		if (full) {
			for (int rr = r; rr > 0; rr--)
				memcpy(game->board[rr], game->board[rr - 1], TETRIS_COLS);
			memset(game->board[0], 0, TETRIS_COLS);
			cleared++;
		} else {
			r--;
		}
	}
	return cleared;
}
void scoring(tetris_game_t *game, int line_cleared) {
	if (line_cleared < 1 || line_cleared > 4) return;
	game->score += (uint32_t)(scores[line_cleared] * game->level);
	game->lines += line_cleared;
	game->level = (game->lines / 10) + 1;
}
/* Inject game->pending_garbage rows at the bottom: shift settled blocks up,
 * fill each new bottom row solid except one random hole. */
static void apply_garbage(tetris_game_t *game) {
	uint8_t n = game->pending_garbage;
	if (n == 0) return;
	game->pending_garbage = 0;
	if (n > TETRIS_ROWS) n = TETRIS_ROWS;

	/* blocks that would be pushed above the top -> topped out */
	for (int r = 0; r < n; r++)
		for (int c = 0; c < TETRIS_COLS; c++)
			if (game->board[r][c]) { game->game_over = true; }

	for (int r = 0; r < TETRIS_ROWS - n; r++)
		memcpy(game->board[r], game->board[r + n], TETRIS_COLS);
	uint8_t hole = (uint8_t)(tetris_rng_next(game) % TETRIS_COLS);  /* same gap */
	for (int k = 0; k < n; k++) {
		int r = TETRIS_ROWS - n + k;
		for (int c = 0; c < TETRIS_COLS; c++)
			game->board[r][c] = (c == hole) ? TETRIS_EMPTY : TETRIS_GARBAGE;
	}
}

void tetris_queue_garbage(tetris_game_t *game, uint8_t rows) {
	if (game == NULL || rows == 0) return;
	uint16_t q = (uint16_t)game->pending_garbage + rows;
	game->pending_garbage = (q > TETRIS_ROWS) ? TETRIS_ROWS : (uint8_t)q;
}

void lock_piece(tetris_game_t *game) {
	for (int i = 0; i < 4; i++) {
		cell_t c = piece_cell(game->active_piece, game->rotation, i);
		int bx = game->active_x + c.c, by = game->active_y + c.r;
		if (by >= 0) game->board[by][bx] = game->active_piece;
	}
	int cleared = clear_lines(game);
	scoring(game, cleared);
	if (cleared == 0)           /* garbage only lands when you don't clear */
		apply_garbage(game);
	spawn_from_queue(game);
	game->hold_used = false;
}
void tetris_gravity_step(tetris_game_t *game)
{
	if (game->game_over) return;
	if (tetris_can_place(game, game->active_piece, game->rotation, game->active_x, game->active_y + 1)){
		game->active_y++;
	} else {
		lock_piece(game);
	}
}

void tetris_hold(tetris_game_t *game)
{
	if (game->game_over || game->hold_used) return;

	tetris_piece_t current = game->active_piece;
	if (game->held_piece == 0) {
		game->held_piece = current;
		spawn_from_queue(game);
	} else {
		tetris_piece_t swap = game->held_piece;
		game->held_piece = current;
		spawn(game, swap);
	}
	game->hold_used = true;
}

void tetris_snapshot(const tetris_game_t *game,
		uint8_t cells[TETRIS_ROWS * TETRIS_COLS])
{
	if (game == NULL || cells == NULL)
		return ;
	memcpy(cells, game->board, sizeof(game->board));
	if (game->game_over || game->active_piece == TETRIS_EMPTY) return;

	for (int i = 0; i < 4; i++) {
		cell_t c = piece_cell(game->active_piece, game->rotation, i);
		int col = game->active_x + c.c;
		int row = game->active_y + c.r;

		if (row < 0 || row >=  TETRIS_ROWS || col < 0 || col >= TETRIS_COLS) continue;

		cells[row * TETRIS_COLS + col] = (uint8_t)game->active_piece;
	}
}
uint16_t tetris_gravity_interval_ms(uint8_t level)
{
	static const uint8_t frames_per_row[30] = {
		48, 43, 38, 33, 28, 23, 18, 13,  8,  6,   /* levels 0-9   */
		 5,  5,  5,  4,  4,  4,  3,  3,  3,  2,   /* levels 10-19 */
		 2,  2,  2,  2,  2,  2,  2,  2,  2,  1    /* levels 20-29 */
	};

	if (level > 30)
		level = 30;                              /* clamp: 30+ is max speed */

	/* frame = 1000 / 60.0988 ms ~= 16.639 ms; round to nearest */
	return (uint16_t)(((uint32_t)frames_per_row[level - 1] * 16639 + 500) / 1000);
}
