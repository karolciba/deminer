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
unsigned long per_thread = 0;

/*
 * Global file descriptor for database file
 */
int fd;

/*
 * Structure for state probability
 * TODO: Real probability to calcuated in real time vs. storing rescaled score
 * in structure.
 */
typedef struct {
	unsigned int mines;
	unsigned int tries;
	unsigned int score; /* probability of mine scaled to UINT_MAX */
} state_p;

/*
 * Global pointer to mmaped database file
 */
state_p *db;

/*
 * Mutex for concurrent accessing database
 */
pthread_mutex_t lock;

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
unsigned int hash(int *window) {
	unsigned int code = 0;

	code = window[0];
	code = code * 11 + window[1];
	code = code * 11 + window[2];
	code = code * 11 + window[3];
	/* code += 0x10000 * window[4]; */
	code = code * 11 + window[5];
	code = code * 11 + window[6];
	code = code * 11 + window[7];
	code = code * 11 + window[8];

	return code;
}

/*
 * This hash is one to one relation, we can unwind hash to
 * see what window it is calculated for. Can be used to
 * inspect database.
 */
void unhash(int *window, unsigned int code) {
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

static inline void window(int *window, board *b, int index) {
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

/* 
 * Open database (creates if not available)
 * and set global pointer for database.
 */
void init_db() {
	int new_db = 0;
	unsigned int size = 0;
	int result;
	size = COMBINATIONS * sizeof(state_p);
	
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
		printf("DB_FILE corrupted size %li expected %u\n", statbuf.st_size, size);
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


	/*
	 * Initial state smoothed
	 * assuming there was one try for each possibility
	 * mines: 1, tries: 2
	 * precalculate score to be 50%
	 */
	if (new_db) {
		printf("DB was currupted, resseting state\n");
		for (unsigned int i = 0; i < COMBINATIONS; i++) {
			db[i] = (state_p){ 1, 2, UINT_MAX/2 };
		}
	}

	/* msync(db, size, MS_SYNC | MS_INVALIDATE); */
}

/*
 * Close database file, sync and un-map memory file.
 */
void close_db() {
	int size = COMBINATIONS * sizeof(state_p);
	msync(db, size, MS_SYNC | MS_INVALIDATE);
	munmap(db, size);
	close(fd);
}

/* 
 * Checks probability of mine for given configuration
 */
static inline unsigned int check(int *window) {
	unsigned int code = hash(window);
	return db[code].score;
	/* unsigned int mines = db[code]; */
	/* unsigned int tries = db[code+1]; */
	/* return ( (float) mines * UINT_MAX ) / (float) tries; */
}


/*
 * Returns tries count for give state. Used in estimation
 * of "validity" of score
 */
static inline unsigned int tries(int *window) {
       unsigned int code = hash(window);
       return db[code].tries;
}


/*
 * Updates probabilites for given configuration
 */
static inline void update(int *window, int mine) {
	unsigned int code = hash(window);

	if (db[code].mines < UINT_MAX && db[code].tries < UINT_MAX) {
		pthread_mutex_lock(&lock);
		if (mine) {
			db[code].mines++;
		}
		db[code].tries++;
		db[code].score = ( (unsigned long) db[code].mines * UINT_MAX ) / (unsigned long)db[code].tries;
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
 * Train data model.
 * Prepare simulation of game using current model database
 * update probabilities along way.
 */
int random_train(int rows, int cols, int mines) {
	board *b = init_board(rows,cols,mines);

	/* int *probmap; */
       	/* probmap = malloc(rows*cols,*sizeof(int)); */

	int ret = 0;
	int *w = alloca(9 * sizeof(int));

	/*
	 * Using this big alloca in threads gives
	 * nasty segfaults
	 */
	int *fields = malloc(b->size*sizeof(int)+1);
	if (!fields) {
		perror("Can't create stack");
	}
	int top = 0;

	while (1) {
		top = 0;
		for (int i = 0; i < b->size; i++) {
			if (b->visible[i] == 1)
				continue;
			fields[top++] = i;
		}

		/* randomly select index from available stack */
		int i = rand()%(top+1);
		int index = fields[i];
		
		window(w, b, index);
		ret = uncover(b, index, -1);

		if (ret == -1) {
			update(w, 1);
		}
		if (ret == 0) {
			update(w, 0);
		}

		if (ret != 0) break;
	}

	free(fields);
	destroy_board(b);
	return ret;
}


/*
 * Train data model.
 * Prepare simulation of game using current model database
 * update probabilities along way.
 *
 * Favors solution with low try count.
 */
int favor_train(int rows, int cols, int mines) {
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
                       score = tries(w);
                       /* only calculate real score for
                        * tests with more than 1,000,000 tries
                        * (0.2% mine probability)
                        * less tries - move favor
                        * TODO: consider smoother transition
                        */
                       if (score > 1000000) score = check(w);
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
	long thread_id = (long int)ignore;

	unsigned long plays = per_thread;
	int wins = 0;
	for (int i = 0; i < plays; i++) {
		if (i%1000 == 0) {
			printf("\r%li Train %d", thread_id, i);
			fflush(stdout);
		}
		if (train(ROWS,COLS,MINES) == 1)
			wins++;
	}

	printf("\nAfter %li was %d wins\n", plays, wins);
	return NULL;
}

/*
 * Worker thread for solving simulation in parallel,
 * using random selection of fields to be uncovered
 * to avoid bias from low tries.
 */
void *random_solver_thread(void *ignore) {
	long thread_id = (long)ignore;

	unsigned long plays = per_thread;
	int wins = 0;
	for (int i = 0; i < plays; i++) {
		if (i%1000 == 0) {
			printf("\r%li Train %d", thread_id, i);
			fflush(stdout);
		}
		if (random_train(ROWS,COLS,MINES) == 1)
			wins++;
	}

	printf("\nAfter %li was %d wins\n", plays, wins);
	return NULL;
}

/*
 * Worker thread for solving simulation in parallel,
 * using random selection of fields to be uncovered
 * to avoid bias from low tries.
 */
void *favor_solver_thread(void *ignore) {
	long thread_id = (long)ignore;

	unsigned long plays = per_thread;
	int wins = 0;
	for (int i = 0; i < plays; i++) {
		if (i%1000 == 0) {
			printf("\r%li Train %d", thread_id, i);
			fflush(stdout);
		}
		if (favor_train(ROWS,COLS,MINES) == 1)
			wins++;
	}

	printf("\nAfter %li was %d wins\n", plays, wins);
	return NULL;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage:\n");
		printf("play single simulation: %s p\n", argv[0]);
		printf("train: %s t NUMBER_OF_SIMULATIONS\n", argv[0]);
		printf("one thread train: %s st NUMBER_OF_SIMULATIONS\n", argv[0]);
		printf("random train: %s r NUMBER_OF_SIMULATIONS\n", argv[0]);
		printf("one thread random train: %s sr NUMBER_OF_SIMULATIONS\n", argv[0]);
		printf("favor train: %s f NUMBER_OF_SIMULATIONS\n", argv[0]);
		printf("one thread favor train: %s sf NUMBER_OF_SIMULATIONS\n", argv[0]);
		printf("inspect db: %s d\n", argv[0]);
		exit(1);
	}

	init_db();

	if (strcmp(argv[1], "d") == 0) {
		int * w = alloca(9 * sizeof(int));
		for (int i = 0; i < COMBINATIONS; i++) {
			if (db[i].mines > 1 && db[i].tries > 2) {
				unhash(w, i);
				print_window(w);
				printf("Index %#x %d/%d = %u %f\n", i,db[i].mines,db[i].tries, db[i].score, (float)db[i].score/UINT_MAX);
			}
				
		}
		exit(0);
	}

	pthread_t *threads = alloca(THREADS * sizeof(pthread_t));
	if (strcmp(argv[1], "t") == 0) {
		unsigned long plays = atol(argv[2]);
		per_thread = plays/THREADS;
		// thread_create
		for (long i = 0; i < THREADS; i++) {
			pthread_create( &threads[i], NULL, solver_thread, (void*) i);
		}
		for (int i = 0; i < THREADS; i++) {
			pthread_join( threads[i], NULL);
		}
	}

	if (strcmp(argv[1], "r") == 0) {
		unsigned long plays = atol(argv[2]);
		per_thread = plays/THREADS;
		// thread_create
		for (long i = 0; i < THREADS; i++) {
			pthread_create( &threads[i], NULL, random_solver_thread, (void*) i);
		}
		for (int i = 0; i < THREADS; i++) {
			pthread_join( threads[i], NULL);
		}
	}

	if (strcmp(argv[1], "f") == 0) {
		unsigned long plays = atol(argv[2]);
		per_thread = plays/THREADS;
		// thread_create
		for (long i = 0; i < THREADS; i++) {
			pthread_create( &threads[i], NULL, favor_solver_thread, (void*) i);
		}
		for (int i = 0; i < THREADS; i++) {
			pthread_join( threads[i], NULL);
		}
	}

	if (strcmp(argv[1], "st") == 0) {
		int plays = atoi(argv[2]);
		int wins = 0;
		for (int i = 0; i < plays; i++) {
			if (i%1000 == 0) {
				printf("\rTrain %d", i);
				fflush(stdout);
			}
			if (train(ROWS,COLS,MINES) == 1)
				wins++;
		}

		printf("\nAfter %d was %d wins\n", plays, wins);
	}

	if (strcmp(argv[1], "sr") == 0) {
		int plays = atoi(argv[2]);
		int wins = 0;
		for (int i = 0; i < plays; i++) {
			if (i%1000 == 0) {
				printf("\rTrain %d", i);
				fflush(stdout);
			}
			if (random_train(ROWS,COLS,MINES) == 1)
				wins++;
		}

		printf("\nAfter %d was %d wins\n", plays, wins);
	}

	if (strcmp(argv[1], "sf") == 0) {
		int plays = atoi(argv[2]);
		int wins = 0;
		for (int i = 0; i < plays; i++) {
			if (i%1000 == 0) {
				printf("\rTrain %d", i);
				fflush(stdout);
			}
			if (favor_train(ROWS,COLS,MINES) == 1)
				wins++;
		}

		printf("\nAfter %d was %d wins\n", plays, wins);
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
