
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

int flat(board *b, int row, int col);
void dim(board *b, int index, int *row, int *col);

int field(board *b, int index);
void window(int *window, board *b, int index);
void print_window(int *window);

int up(board *b, int index);
int down(board *b, int index);

int left(board *b, int index);
int left_up(board *b, int index);
int left_down(board *b, int index);

int right(board *b, int index);
int right_up(board *b, int index);
int right_down(board *b, int index);

int uncover(board *b, int row, int col);

board *init_board(int rows, int cols, int mines);
void print_board(board *b, int debug);
void destroy_board(board *b);
