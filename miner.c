#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <alloca.h>
#include "miner.h"


int flat(board *b, int row, int col) {
	return b->cols*row+col;
}

int up(board *b, int index) {
	return index - b->cols;
}
int down(board *b, int index) {
	int i = index + (b->cols);
	if (i < b->size)
		return i;
	else
		return -1;
}
int left(board *b, int index) {
	if (0 == index % (b->cols))
		return -1;
	else
		return  index - 1;
}
int right(board *b, int index) {
	if (0 == (index + 1) % (b->cols))
		return -1;
	else
		return index + 1;
}
int left_up(board *b, int index) {
	int i = left(b, index);
	if (i > 0)
		return up(b, i);
	else
		return -1;
}
int left_down(board *b, int index) {
	int i = left(b, index);
	if (i > 0)
		return down(b, i);
	else
		return -1;
}
int right_up(board *b, int index) {
	int i = right(b, index);
	if (i > 0)
		return up(b, i);
	else
		return -1;
}
int right_down(board *b, int index) {
	int i = right(b, index);
	if (i > 0)
		return down(b, i);
	else
		return -1;
}

void dim(board *b, int index, int * row, int *col) {
	*row = index / (b->cols);
	*col = index % (b->cols);
}

int field(board *b, int index) {
	if (index < 0) {
		return none;
	}
	if (b->visible[index] == 0) {
		return unknown;
	}
	
	return (b->matrix[index]);
}

void window(int *window, board *b, int index) {
	int i0, i1, i2, i3, i5, i6, i7, i8 ;
	i0 = left_up(b, index);
	window[0] = field(b, i0);
	i1 = up(b, index);
	window[1] = field(b, i1);
	i2 = right_up(b, index);
	window[2] = field(b, i2);
	i3 = left(b, index);
	window[3] = field(b, i3);
	// i4 = index;
	window[4] = field(b, index);
	i5 = right(b, index);
	window[5] = field(b, i5);
	i6 = left_down(b, index);
	window[6] = field(b, i6);
	i7 = down(b, index);
	window[7] = field(b, i7);
	i8 = right_down(b, index);
	window[8] = field(b, i8);
}

void print_field(int field) {
	switch(field) {
		case zero:
			putchar(' ');
			break;
		case one:
			putchar('1');
			break;
		case two:
			putchar('2');
			break;
		case three:
			putchar('3');
			break;
		case four:
			putchar('4');
			break;
		case five:
			putchar('5');
			break;
		case six:
			putchar('6');
			break;
		case seven:
			putchar('7');
			break;
		case eight:
			putchar('8');
			break;
		case mine:
			putchar('*');
			break;
		case unknown:
			putchar('#');
			break;
		case none:
			putchar('_');
			break;
		default:
			putchar('?');
	}
}

void print_window(int *window) {
	print_field(window[0]);
	print_field(window[1]);
	print_field(window[2]);
	printf("\n");
	print_field(window[3]);
	print_field(window[4]);
	print_field(window[5]);
	printf("\n");
	print_field(window[6]);
	print_field(window[7]);
	print_field(window[8]);
	printf("\n");
}

board *init_board(int rows, int cols, int mines) {
	board *new_board = malloc(sizeof(board));
	
	// store info about board
	new_board->rows = rows;
	new_board->cols = cols;
	new_board->mines = mines;
	new_board->size = rows*cols;
	new_board->left = rows*cols;

	// matrix which will hold info about mines
	int *matrix = calloc(rows*cols, sizeof(int));
	new_board->matrix = matrix;

	// mask which hold info about visible tiles
	int *visible = calloc(rows*cols, sizeof(int));
	new_board->visible = visible;

	// randomely distribute mines
	int size = rows*cols;
	int * stack = malloc(10000*rows*cols*sizeof(int));
	// initialize stack with tiles numbers
	for (int i = 0; i < size; i++) {
		stack[i] = i;
	}
	// random mine placements
	srand((int)time(NULL));
	for (int i = 0; i < mines; i++) {
		int index = i + rand()%(size - i);
		int tmp = stack[index];
		stack[index] = stack[i];
		stack[i] = tmp;
	}

	for (int i = 0; i < mines; i++) {
		int index = stack[i];
		new_board->matrix[index] = mine;
		int upi = up(new_board, index);
		if (upi >= 0) {
			if (new_board->matrix[upi] <= seven) {
				new_board->matrix[upi]++;
			}
		}
		int leftupi = left_up(new_board, index);
		if (leftupi >= 0) {
			if (new_board->matrix[leftupi] <= seven) {
				new_board->matrix[leftupi]++;
			}
		}
		int rightupi = right_up(new_board, index);
		if (rightupi >= 0) {
			if (new_board->matrix[rightupi] <= seven) {
				new_board->matrix[rightupi]++;
			}
		}
		int downi = down(new_board, index);
		if (downi >= 0) {
			if (new_board->matrix[downi] <= seven) {
				new_board->matrix[downi]++;
			}
		}
		int leftdowni = left_down(new_board, index);
		if (leftdowni >= 0) {
			if (new_board->matrix[leftdowni] <= seven) {
				new_board->matrix[leftdowni]++;
			}
		}
		int rightdowni = right_down(new_board, index);
		if (rightdowni >= 0) {
			if (new_board->matrix[rightdowni] <= seven) {
				new_board->matrix[rightdowni]++;
			}
		}
		int lefti = left(new_board, index);
		if (lefti >= 0) {
			if (new_board->matrix[lefti] <= seven) {
				new_board->matrix[lefti]++;
			}
		}
		int righti = right(new_board, index);
		if (righti >= 0) {
			if (new_board->matrix[righti] <= seven) {
				new_board->matrix[righti]++;
			}
		}
	}
	free(stack);

	return new_board;
}

int uncover(board *b, int row, int col) {
	int index;
	if (col > 0)
		index = flat(b, row, col);
	else
		index = row;


	if (b->matrix[index] == mine) {
		b->visible[index] = 1;
		b->left--;
		return -1; // player lost
	}

	if (b->matrix[index] <= eight) {
		// TODO: fix to big stack - check stack before putting new
		int *stack = calloc(1*b->size, sizeof(int));
		int top=0, i, upi, leftupi, rightupi, downi, leftdowni, rightdowni, lefti, righti;
		stack[top++] = index;
		while (top > 0) {
			i = stack[--top];
			if (b->visible[i] == 1)
				continue; // already visited

			b->left--;
			b->visible[i] = 1;
			if (b->matrix[i] == zero) {
				upi = up(b, i);
				if (upi >= 0 && b->visible[upi]==0)// && b->matrix[upi] == zero)
					stack[top++] = upi;
				/* leftupi = left_up(b, i); */
				/* if (leftupi >= 0 && b->visible[leftupi]==0)// && b->matrix[leftupi] == zero) */
				/* 	stack[top++] = leftupi; */
				/* rightupi = right_up(b, i); */
				/* if (rightupi >= 0 && b->visible[rightupi]==0)// && b->matrix[rightupi] == zero) */
				/* 	stack[top++] = rightupi; */
				downi = down(b, i);
				if (downi >= 0 && b->visible[downi]==0) // && b->matrix[downi] == zero)
					stack[top++] = downi;
				/* leftdowni = left_down(b, i); */
				/* if (leftdowni >= 0 && b->visible[leftdowni]==0) // && b->matrix[leftdowni] == zero) */
				/* 	stack[top++] = leftdowni; */
				/* rightdowni = right_down(b, i); */
				/* if (rightdowni >= 0 && b->visible[rightdowni]==0) // && b->matrix[rightdowni] == zero) */
				/* 	stack[top++] = rightdowni; */
				lefti = left(b, i);
				if (lefti >= 0 && b->visible[lefti]==0) // && b->matrix[lefti] == zero)
					stack[top++] = lefti;
				righti = right(b, i);
				if (righti >= 0 && b->visible[righti]==0) // && b->matrix[righti] == zero)
					stack[top++] = righti;
			}
		}
		free(stack);
	}
	
	if (b->mines == b->left) {
		return 1; // player won
	}

	return 0; // game still playing
}

void print_board(board *b, int debug) {
	printf("board %dx%d size %d mines %d left %d\n", b->rows, b->cols, b->size, b->mines, b->left);

	putchar(' ');
	putchar(' ');
	for (int i = 0; i < b->cols; i++) {
		printf("%d", i%10);
	}
	putchar('\n');
	putchar(' ');
	putchar(' ');
	for (int i = 0; i < b->cols; i++) {
		putchar('-');
	}


	for (int i = 0; i < b->size; i++) {
		if ( 0 == (i)%(b->cols) ) {
			printf("\n%d|", ((i+1)/(b->cols))%10 );
		}

		if (debug == 0 && b->visible[i] == 0) {
			putchar('#');
			continue;
		}

		switch(b->matrix[i]) {
			case zero:
				putchar(' ');
				break;
			case one:
				putchar('1');
				break;
			case two:
				putchar('2');
				break;
			case three:
				putchar('3');
				break;
			case four:
				putchar('4');
				break;
			case five:
				putchar('5');
				break;
			case six:
				putchar('6');
				break;
			case seven:
				putchar('7');
				break;
			case eight:
				putchar('8');
				break;
			case mine:
				putchar('*');
				break;
			default:
				putchar('?');
		}
		
	}

	putchar('\n');
}

void destroy_board(board *b) {
	free(b->visible);
	free(b->matrix);
	free(b);
}

