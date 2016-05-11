#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <alloca.h>
#include <pthread.h>

#include "miner.h"

/*
 * 3 by 3 window,
 * each field in possible state:
 * 0,1,2,3,4,5,6,7,8,UNKNOWN,NONE
 * where:
 * UNKNOWN - field yet uncovered
 * NONE - field outside board (corner, side)
 * eg.
 *
 *  ___
 *  _#1
 *  _12
 *
 * center field is always unknown and under consideration
 * that meand there are 11(states)**8(filed) possible states
 */
#define COMBINATIONS  214358881
/*
 * Database will contain COMBINATIONS * sizeof(state_p)
 * COUNT should be at least 4 bytes int. As short type will give
 * only 65535 tries, which is very small number in comparison to
 * possible combination and needed tries to get fair estimation
 * of probability of mine in given combination.
 */
#define DB_FILE "brutesolver.db"

/*
 * Expert setting in Microsoft minesweeper
 */
#define ROWS 16
#define COLS 16
#define MINES 40

/*
 * Let's use those contemporary multicore processors. Calculation
 * bilions of combinations takes days.
 */
#define THREADS 8
/*
 * Global to pass information how many simulation each thread should
 * calculate
 */
int per_thread = 0;

/*
 * Global file descriptor for database file
 */
int fd;

/*
 * Global pointer to mmaped database file
 */
unsigned int *db;

/*
 * Mutex for concurrent accessing database
 */
pthread_mutex_t lock;

/*
 * Structure for state probability
 * TODO: Real probability to calcuated in real time vs. storing rescaled score
 * in structure.
 */
typedef struct {
	int mines;
	int tries;
} state_p;


/* 
 * Assumes 3by3 window, middle element is always unknown
 * will have much empty space, as there are only 11 posible
 * states for field, and it's using 4 bits to encode each.
 *
 * Faster approach could be using 4 bites per field, full state
 * would have 32 bit hash, occupying whole int.
 * This would give faster hash calcuation (only bit shifting:
 * code<<4
 * code&=window[i]
 * ) however in expense for bigger (mostly empty) database file.
 * 12 GB instead of 1.6 GB
 */
int hash(int *window) {
	int code = 0;

	code = window[0];
	code = code * 11 + window[1];
	code = code * 11 + window[2];
	code = code * 11 + window[3];
	/* code += 0x10000 * window[4]; */
	code = code * 11 + window[5];
	code = code * 11 + window[6];
	code = code * 11 + window[7];
	code = code * 11 + window[8];

	/* TODO: refactor to use struct and indexing it not ints */
	code *= 2;

	return code;
}

/*
 * This hash is one to one relation, we can unwind hash to
 * see what window it is calculated for. Can be used to
 * inspect database.
 */
void unhash(int *window, int code) {
	/* TODO: refactor to use struct and indexing it not ints */
	code /= 2;

	window[8] = code % 11;
	code /= 11;
	window[7] = code % 11;
	code /= 11;
	window[6] = code % 11;
	code /= 11;
	window[5] = code % 11;
	code /= 11;
	window[3] = code % 11;
	code /= 11;
	window[2] = code % 11;
	code /= 11;
	window[1] = code % 11;
	code /= 11;
	window[0] = code;

	window[4] = unknown;
}

/* 
 * Open database (creates if not available)
 * and set global pointer for database.
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

/*
 * Close database file, sync and un-map memory file.
 */
void close_db() {
	int size = COMBINATIONS * sizeof(unsigned int) * 2;
	msync(db, size, MS_SYNC | MS_INVALIDATE);
	munmap(db, size);
	close(fd);
}

/* 
 * Checks probability of mine for given configuration
 */
unsigned int check(int *window) {
	int code = hash(window);
	unsigned int mines = db[code];
	unsigned int tries = db[code+1];
	return ( (float) mines * UINT_MAX ) / (float) tries;
}

/*
 * Updates probabilites for given configuration
 */
void update(int *window, int mine) {
	int code = hash(window);

	if (db[code] < UINT_MAX && db[code+1] < UINT_MAX) {
		pthread_mutex_lock(&lock);
		if (mine) {
			db[code]++;
		}
		db[code+1]++;
		pthread_mutex_unlock(&lock);
	}
}

/*
 * Train data model.
 * Prepare simulation of game using current model database
 * update probabilities along way.
 */
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

/*
 * Play game using current database,
 * do not update probabilities
 */
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
		print_board(b,0);

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

/*
 * Worker thread for solving simulation in parallel
 */
void *solver_thread(void *ignore) {
	pthread_t self;
	self = pthread_self();

	int plays = per_thread;
	int wins = 0;
	for (int i = 0; i < plays; i++) {
		if (i%1000 == 0) {
			printf("\r%p Train %d", (void *)self, i);
			fflush(stdout);
		}
		if (train(ROWS,COLS,MINES) == 1)
			wins++;
	}

	printf("\nAfter %d was %d wins\n", plays, wins);
	return NULL;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage:\n");
		printf("play single simulation: %s p\n", argv[0]);
		printf("train: %s t NUMBER_OF_SIMULATIONS\n", argv[0]);
		printf("inspect db: %s d\n", argv[0]);
		exit(1);
	}

	init_db();

	if (strcmp(argv[1], "d") == 0) {
		int * w = alloca(9 * sizeof(int));
		for (int i = 0; i < COMBINATIONS; i++) {
			if (db[2*i] > 99 && db[2*i+1] > 100) {
				unhash(w, 2*i);
				print_window(w);
				printf("Index %#x %d/%d = %f\n", i,db[2*i],db[2*i+1], (float)db[2*i]/(float)db[2*i+1]);
			}
				
		}
		exit(0);
	}

	pthread_t *threads = alloca(THREADS * sizeof(pthread_t));
	if (strcmp(argv[1], "t") == 0) {
		int plays = atoi(argv[2]);
		per_thread = plays/THREADS;
		// thread_create
		for (int i = 0; i < THREADS; i++) {
			pthread_create( &threads[i], NULL, solver_thread, (void*) NULL);
		}
		for (int i = 0; i < THREADS; i++) {
			pthread_join( threads[i], NULL);
		}
	}

	if (strcmp(argv[1], "p") == 0) {
		int ret;
		ret = play(ROWS,COLS,MINES);
		if (ret == 1)
			printf("You won!\n");
		else
			printf("You lost!\n");
	}

	close_db();
}
