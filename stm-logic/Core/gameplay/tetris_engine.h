/** Platform-independent Tetris rules. No HAL, buttons, UART, or delays. */
#ifndef TETRIS_ENGINE_H
# define TETRIS_ENGINE_H

# include <stdbool.h>
# include <stdint.h>

# define TETRIS_ROWS 20
# define TETRIS_COLS 10
# define TETRIS_PIECE_COUNT 7

/* Values also serve as the UART display's cell/color codes. */
typedef enum
{
	TETRIS_EMPTY = 0,
	TETRIS_I,
	TETRIS_O,
	TETRIS_T,
	TETRIS_S,
	TETRIS_Z,
	TETRIS_J,
	TETRIS_L
} tetris_piece_t;

typedef struct
{
	/* Settled blocks only. The active piece must never be stored here until lock. */
	uint8_t board[TETRIS_ROWS][TETRIS_COLS];
	tetris_piece_t active_piece;
	uint8_t rotation; /* 0..3 clockwise turns from the base shape */
	int8_t active_x; /* Column of the piece-local box origin */
	int8_t active_y; /* Row of the piece-local box origin */
	tetris_piece_t next_piece; /* Exactly one preview */
	tetris_piece_t held_piece; /* TETRIS_EMPTY means no held piece */
	bool hold_used; /* Reset only after a piece locks and the next one spawns */
	bool game_over;
	uint32_t score;
	uint16_t lines;
	uint8_t level; /* Starts at 1; increases every 10 lines */
	uint32_t rng_state;
} tetris_game_t;

/* Initialize all fields, then draw an active piece and one next piece.
 * For repeatable debugging, the same seed must produce the same sequence.
 */
void tetris_init(tetris_game_t *game, uint32_t seed);

/* Attempt one action. Invalid moves and actions after game_over are no-ops.
 * Rotation is counterclockwise with no kicks: reject an invalid candidate.
 */
void tetris_move_left(tetris_game_t *game);
void tetris_move_right(tetris_game_t *game);
void tetris_rotate_left(tetris_game_t *game);

/* Once per falling piece. Empty slot consumes next; swapping does not.
 * Reset the incoming piece's position/rotation and check spawn collision.
 */
void tetris_hold(tetris_game_t *game);

/* One gravity event: descend one row, or lock -> clear -> score -> spawn.
 * The caller controls elapsed time; this function must not wait or read a clock.
 */
void tetris_gravity_step(tetris_game_t *game);

/* Pure query: candidate placement must not alter the game.
 * Check occupied shape cells, not the whole bounding box.
 * Cells above row 0 are allowed; walls, floor, and settled overlaps are not.
 */
bool tetris_can_place(const tetris_game_t *game, tetris_piece_t piece,
		uint8_t rotation, int8_t x, int8_t y);

/* Output 200 row-major color codes: settled blocks plus visible active cells.
 * This is a display copy; do not modify game->board. Clip cells above the top.
 * After game over, show settled blocks rather than an invalid spawn overlay.
 */
void tetris_snapshot(const tetris_game_t *game,
		uint8_t cells[TETRIS_ROWS * TETRIS_COLS]);
/* Advance caller-owned PRNG state; same starting state -> same sequence.
 * A zero state must be replaced with a nonzero default when using xorshift.
 */
uint32_t tetris_rng_next(tetris_game_t *game);

/* Independent choice in 1..7, with replacement (not a shuffled 7-bag).
 * Repeated pieces are expected. Do not reseed for each draw.
 */
tetris_piece_t tetris_rng_piece(tetris_game_t *game);

uint16_t tetris_gravity_interval_ms(uint8_t level);

#endif /* TETRIS_ENGINE_H */
