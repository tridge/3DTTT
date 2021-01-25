#define MAX_N 8
#define MAX_SQUARES (MAX_N * MAX_N * MAX_N)
#define PIECE_NONE 0
#define PIECE_A 1
#define PIECE_B -1
#define PIECE_SUGGEST 2

struct state {
	char board[MAX_SQUARES];
	int game_record[MAX_SQUARES];
	volatile int next_to_play;
	int computer;
	int winx, winy, winz;
	int windx, windy, windz;
	int winner;
	int play_level;
	int moved;
	int n;
	int show_search;
	int num_moves;
	int quit;
};

