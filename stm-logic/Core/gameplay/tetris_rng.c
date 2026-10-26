#include "tetris_engine.h"

uint32_t tetris_rng_next(tetris_game_t *game)
{
    uint32_t x = game->rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return game->rng_state = x;
}

tetris_piece_t tetris_rng_piece(tetris_game_t *game)
{
    tetris_piece_t p = (tetris_piece_t)(tetris_rng_next(game) % 7 + 1);
    return p;
}
