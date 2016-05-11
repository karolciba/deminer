#ifndef MINER_H
#define MINER_H

typedef enum FIELDS { zero = 0, one, two, three, four, five, six, seven, eight, unknown, none, mine} fields;

typedef struct {
	int *matrix; // NxM map
	int *visible; // NxM filter of visible fields
	int rows;
	int cols;
	int size;
	int mines;
	int left;
} board;

// int flat(board *b, int row, int col);
void dim(board *b, int index, int *row, int *col);

// int field(board *b, int index);
// void window(int *window, board *b, int index);
void print_window(int *window);

// int up(board *b, int index);
// int down(board *b, int index);

// int left(board *b, int index);
// int left_up(board *b, int index);
// int left_down(board *b, int index);

// int right(board *b, int index);
// int right_up(board *b, int index);
// int right_down(board *b, int index);

int uncover(board *b, int row, int col);

board *init_board(int rows, int cols, int mines);
void print_board(board *b, int debug);
void destroy_board(board *b);


/* INLINES */

inline int flat(board *b, int row, int col) {
	return b->cols*row+col;
}

inline int up(board *b, int index) {
	return index - b->cols;
}
inline int down(board *b, int index) {
	int i = index + (b->cols);
	if (i < b->size)
		return i;
	else
		return -1;
}
inline int left(board *b, int index) {
	if (0 == index % (b->cols))
		return -1;
	else
		return  index - 1;
}
inline int right(board *b, int index) {
	if (0 == (index + 1) % (b->cols))
		return -1;
	else
		return index + 1;
}
inline int left_up(board *b, int index) {
	int i = left(b, index);
	if (i > 0)
		return up(b, i);
	else
		return -1;
}
inline int left_down(board *b, int index) {
	int i = left(b, index);
	if (i > 0)
		return down(b, i);
	else
		return -1;
}
inline int right_up(board *b, int index) {
	int i = right(b, index);
	if (i > 0)
		return up(b, i);
	else
		return -1;
}
inline int right_down(board *b, int index) {
	int i = right(b, index);
	if (i > 0)
		return down(b, i);
	else
		return -1;
}

inline int field(board *b, int index) {
	if (index < 0) {
		return none;
	}
	if (b->visible[index] == 0) {
		return unknown;
	}
	
	return (b->matrix[index]);
}
#endif
