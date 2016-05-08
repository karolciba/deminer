#include <stdio.h>
#include <assert.h>
#include "miner.h"


void test_up() {
	printf("Testing up()\n");

	board * b = init_board(8,8,10);

	assert( up(b,  9) ==  1);
	assert( up(b, 31) == 23);
	assert( up(b, 38) == 30);
	assert( up(b, 63) == 55);
	assert( up(b,  8) ==  0);
	assert( up(b,  4) <= -1);
	assert( up(b,  0) <= -1);
}

void test_down() {
	printf("Testing down()\n");

	board *b = init_board(8,8,10);

	assert( down(b,  0) ==  8);
	assert( down(b,  7) == 15);
	assert( down(b, 16) == 24);
	assert( down(b, 35) == 43);
	assert( down(b, 56) <= -1);
	assert( down(b, 60) <= -1);
	assert( down(b, 63) <= -1);

}

void test_left() {
	printf("Testing left()\n");

	board *b = init_board(8,8,10);

	assert( left(b,  0) <= -1);
	assert( left(b,  4) ==  3);
	assert( left(b, 16) <= -1);
	assert( left(b, 60) <= 59);
	assert( left(b, 63) <= 62);

}

void test_right() {
	printf("Testing right()\n");

	board *b = init_board(8,8,10);

	assert( right(b,  0) ==  1);
	assert( right(b,  7) <= -1);
	assert( right(b,  4) <=  5);
	assert( right(b, 60) <= 61);
	assert( right(b, 63) <= -1);

}

void test_flat() {
	printf("Testing flat()\n");

	board *b = init_board(8,8,10);

	assert( flat(b,  0,  0) ==  0);
	assert( flat(b,  1,  0) ==  8);
	assert( flat(b,  7,  7) == 63);

}

void test_print_board() {
	printf("Testing print_board()\n");

	board *b = init_board(4, 4, 1);

	b->matrix = (int[]){ 9, 0, 1, 2, 3, 9, 4, 5, 6, 7, 9, 8, 0, 1, 2, 9 };
	print_board(b, 1);

}

int main(int argc, char **argv) {
	test_up();
	test_down();
	test_left();
	test_right();
	test_flat();
	test_print_board();

	return 0;
}
