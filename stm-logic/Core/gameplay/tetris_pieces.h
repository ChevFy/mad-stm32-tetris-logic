#ifndef TETRIS_PIECES_H
# define TETRIS_PIECES_H

# include "tetris_engine.h"

typedef struct { int8_t r, c; } cell_t;
static const uint8_t box_size[8] = {0, 4, 2, 3, 3, 3, 3, 3};

cell_t piece_cell(tetris_piece_t piece, uint8_t rotation,uint8_t idx);

#endif /* TETRIS_PIECES_H */
