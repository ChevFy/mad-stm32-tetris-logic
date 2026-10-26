#include "tetris_pieces.h"

static const cell_t base_shape[8][4] = {
	{{0,0},{0,0},{0,0},{0,0}},          /* 0 unused */
	{{1,0},{1,1},{1,2},{1,3}},          /* I */
	{{0,0},{0,1},{1,0},{1,1}},          /* O */
	{{0,1},{1,0},{1,1},{1,2}},          /* T */
	{{0,1},{0,2},{1,0},{1,1}},          /* S */
	{{0,0},{0,1},{1,1},{1,2}},          /* Z */
	{{0,0},{1,0},{1,1},{1,2}},          /* J */
	{{0,2},{1,0},{1,1},{1,2}},          /* L */
};
cell_t piece_cell(tetris_piece_t piece, uint8_t rotation,uint8_t idx) {
	cell_t cell = base_shape[piece][idx];
	int n = box_size[piece];
	for (uint8_t i = 0; i < (rotation & 3); i++) {
		int8_t r = cell.r;
		cell.r = cell.c;
		cell.c = (int8_t)(n - 1 - r);
	}
	return cell;
}
