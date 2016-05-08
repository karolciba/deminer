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

void dim(board *b, int index, int * row, int *col) {
	*row = index / (b->cols);
	*col = index % (b->cols);
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
	int * stack = malloc(rows*cols*sizeof(int));
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
			if (new_board->matrix[upi] <= eight) {
				new_board->matrix[upi]++;
			}
		}
		int downi = down(new_board, index);
		if (downi >= 0) {
			if (new_board->matrix[downi] <= eight) {
				new_board->matrix[downi]++;
			}
		}
		int lefti = left(new_board, index);
		if (lefti >= 0) {
			if (new_board->matrix[lefti] <= eight) {
				new_board->matrix[lefti]++;
			}
		}
		int righti = right(new_board, index);
		if (righti >= 0) {
			if (new_board->matrix[righti] <= eight) {
				new_board->matrix[righti]++;
			}
		}
	}

	return new_board;
}

int uncover(board *b, int row, int col) {
	int index = flat(b, row, col);

	if (b->matrix[index] == mine) {
		b->visible[index] = 1;
		b->left--;
		return -1; // player lost
	}

	if (b->matrix[index] <= eight) {
		// TODO: fix to big stack - check stack before putting new
		int *stack = calloc(1*b->size, sizeof(int));
		int top=0, i, upi, downi, lefti, righti;
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
				downi = down(b, i);
				if (downi >= 0 && b->visible[downi]==0) // && b->matrix[downi] == zero)
					stack[top++] = downi;
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

		if (b->visible[i] == 0) {
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

