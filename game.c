#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "miner.h"

int main(int argc, char ** argv) {
	/* board *b = init_board(8,8,10); */
	/* board *b = init_board(16,16,40); */
	/* board *b = init_board(30,16,99); */

	board *b = init_board(16,16,40);
	int row, col, ret;
	while (1) {
		print_board(b, 0);
		printf("input ROW COL to be uncovered\n");
		scanf("%d %d", &row, &col);

		ret = uncover(b, row, col);
		if (ret < 0) {
			print_board(b, 0);
			printf("You lost!\n");
			exit(1);
		}
		if (ret == 1){
			print_board(b, 0);
			printf("You WON!\n");
			exit(1);
		}
	}

}
