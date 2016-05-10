#include "miner.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <alloca.h>

// 11**8
#define COMBINATIONS  214358881
// assuming 4bits per field, and actual 11 combination for last one
// two bytes for nominator, two bytes for denominator
// #define DB_SIZE 11811160064LL
#define DB_SIZE 857435524
#define DB_FILE "brutesolver.db"

int fd;
unsigned int *db;


/* Assumes 3by3 window, middle element is always unknown
 * will have much empty space, as there are only 11 posible
 * states for field, and it's using 4 bits to encode each
 */
int hash(int *window) {
	int code = 0;
	/* code += 0x000000001 * window[0]; */
	/* code += 0x000000010 * window[1]; */
	/* code += 0x000000100 * window[2]; */
	/* code += 0x000001000 * window[3]; */
	/* #<{(| code += 0x10000 * window[4]; |)}># */
	/* code += 0x000010000 * window[5]; */
	/* code += 0x000100000 * window[6]; */
	/* code += 0x001000000 * window[7]; */
	/* code += 0x010000000 * window[8]; */

	code = window[0];
	code = code * 11 + window[1];
	code = code * 11 + window[2];
	code = code * 11 + window[3];
	/* code += 0x10000 * window[4]; */
	code = code * 11 + window[5];
	code = code * 11 + window[6];
	code = code * 11 + window[7];
	code = code * 11 + window[8];

	code *= 2;

	return code;
}

/* Open database (creates if not available)
 * and returns pointer to is
 */
void init_db() {
	int new_db = 0;
	int size = 0;
	int result;
	size = COMBINATIONS * sizeof(unsigned int) * 2;
	
	if( access( DB_FILE, F_OK ) != -1 ) {
		fd = open(DB_FILE, O_RDWR, (mode_t)0600);
		if (fd == -1) {
			perror("Error opening file for writing");
			exit(EXIT_FAILURE);
		}
	} else {
		new_db = 1;
		printf("DB_FILE not existing\n");
		fd = open(DB_FILE, O_RDWR | O_CREAT , (mode_t)0600);
		if (fd == -1) {
			perror("Error opening file for writing");
			exit(EXIT_FAILURE);
		}
	}

	struct stat statbuf;

	fstat(fd, &statbuf);

	// if size different that expected reset db
	if (statbuf.st_size != size) {
		printf("DB_FILE corrupted size %li expected %i\n", statbuf.st_size, size);
		new_db = 1;
	}


	/* #<{(| Stretch the file size to the size of the (mmapped) array of ints */
	/* |)}># */
	result = lseek(fd, size - 1, SEEK_SET);
	if (result == -1) {
		close(fd);
		perror("Error calling lseek() to 'stretch' the file");
		exit(EXIT_FAILURE);
	}

	/* Something needs to be written at the end of the file to
	 * have the file actually have the new size.
	 * Just writing an empty string at the current file position will do.
	 *
	 * Note:
	 *  - The current position in the file is at the end of the stretched 
	 *    file due to the call to lseek().
	 *  - An empty string is actually a single '\0' character, so a zero-byte
	 *    will be written at the last byte of the file.
	 */
	result = write(fd, "", 1);
	if (result != 1) {
		close(fd);
		perror("Error writing last byte of the file");
		exit(EXIT_FAILURE);
	}

	/* Now the file is ready to be mmapped.
	*/
	db = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (db == MAP_FAILED) {
		close(fd);
		perror("Error mmapping the file");
		exit(EXIT_FAILURE);
	}


	if (new_db) {
		printf("DB was currupted, resseting state\n");
		for (unsigned int i = 0; i < COMBINATIONS; i++) {
			db[2*i] = 0;
			db[2*i + 1] = 1;
		}
	}

	/* msync(db, size, MS_SYNC | MS_INVALIDATE); */
}

void close_db() {
	int size = COMBINATIONS * sizeof(unsigned int) * 2;
	/* msync(db, size, MS_SYNC | MS_INVALIDATE); */
	munmap(db, size);
	close(fd);
}

/* Checks probability of mine for given configuration */
unsigned int check(int *window) {
	int code = hash(window);
	unsigned int mines = db[code];
	unsigned int tries = db[code+1];
	return ( (float) mines * UINT_MAX ) / (float) tries;
}

/* Updates probabilites for given configuration */
void update(int *window, int mine) {
	int code = hash(window);
	if (db[code] < UINT_MAX && db[code+1] < UINT_MAX) {
		if (mine)
			db[code]++;
		db[code+1]++;
	}
}


int train(int rows, int cols, int mines) {
	board *b = init_board(rows,cols,mines);

	/* int *probmap; */
       	/* probmap = malloc(rows*cols,*sizeof(int)); */

	int ret = 0;
	unsigned int score;
	unsigned int minimal = UINT_MAX;
	int minimal_i;
	int *w = alloca(9 * sizeof(int));

	while (1) {
		minimal = UINT_MAX;
		for (int i = 0; i < b->size; i++) {
			if (b->visible[i] == 1)
				continue;
			window(w, b, i);
			score = check(w);
			if (score < minimal) {
				minimal = score;
				minimal_i = i;
				/* printf("I: %d, score %d\n",i,score); */
			}
		}
		
		window(w, b, minimal_i);
		ret = uncover(b, minimal_i, -1);

		if (ret == -1) {
			update(w, 1);
		}
		if (ret == 0) {
			update(w, 0);
		}

		if (ret != 0) break;
	}

	destroy_board(b);
	return ret;
}

int play(int rows, int cols, int mines) {
	board *b = init_board(rows,cols,mines);

	/* int *probmap; */
       	/* probmap = malloc(rows*cols,*sizeof(int)); */

	int ret = 0;
	unsigned short score;
	unsigned short minimal = USHRT_MAX;
	int minimal_i;
	int *w = alloca(9 * sizeof(int));

	while (1) {
		minimal = USHRT_MAX;
		for (int i = 0; i < b->size; i++) {
			if (b->visible[i] == 1)
				continue;
			window(w, b, i);
			score = check(w);
			if (score < minimal) {
				minimal = score;
				minimal_i = i;
				printf("I: %d, score %d\n",i,score);
			}
		}
		
		window(w, b, minimal_i);
		ret = uncover(b, minimal_i, -1);
		print_board(b,1);

		if (ret == -1) {
			/* update(w, 1); */
		}
		if (ret == 0) {
			/* update(w, 0); */
		}

		if (ret != 0) break;
	}

	destroy_board(b);
	return ret;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s p|d|t NO_OF_PLAYS\n",argv[0]);
		exit(1);
	}

	init_db();

	printf("Command: %s %s\n",argv[0], argv[1]);
	if (strcmp(argv[1], "d") == 0) {
		for (int i = 0; i < COMBINATIONS; i++) {
			if (db[2*i] != 0 || db[2*i+1] > 1) {
				printf("Index %#x %d/%d\n", i,db[2*i],db[2*i+1]);
			}
				
		}
		exit(0);
	}

	if (strcmp(argv[1], "t") == 0) {
		int plays = atoi(argv[2]);
		int wins = 0;
		for (int i = 0; i < plays; i++) {
			if (i%1000 == 0) {
				printf("\rTrain %d", i);
				fflush(stdout);
			}
			if (train(16,16,40) == 1)
				wins++;
		}
		printf("\nAfter %d was %d wins\n", plays, wins);
	}

	if (strcmp(argv[1], "p") == 0) {
		play(16,16,40);
	}

	close_db();
}
