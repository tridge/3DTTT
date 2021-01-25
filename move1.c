#include <stdio.h>
#include <string.h>
#include "3dttt.h"

#define N 4
#define MAX_LINES 100
#define MAX_LINES_PER_SQUARE 10
#define NUM_SQUARES (N*N*N)
#define MIXED 2
#define WIN (1<<30)

static int num_lines = 0;

typedef int Line[N];

Line lines[MAX_LINES];

static int sq_lines[NUM_SQUARES][MAX_LINES_PER_SQUARE];
static int num_sq_lines[NUM_SQUARES];

#define SQR(x) ((x)*(x))

static int map3dto1d(int x,int y,int z)
{
	return x*N*N + y*N + z;
}

static void map1dto3d(int i, int *x, int *y, int *z)
{
	*z = i%N;
	*y = (i/N)%N;
	*x = (i/(N*N))%N;
}

static char *posstr(int i)
{
	static char ret[30];
	int x,y,z;
	map1dto3d(i, &x, &y, &z);
	sprintf(ret,"(%d,%d,%d)",x,y,z);
	return ret;
}

static void add_line(int x,int y,int z,int dx,int dy,int dz)
{
	int l, i;
	Line newline;

	for (i=0;i<N;i++)
		newline[i] = map3dto1d(x+dx*i,y+dy*i,z+dz*i);

	for (l=0;l<num_lines;l++)
		if (memcmp(newline, lines[l], sizeof(newline)) == 0)
			return;

	for (i=0;i<N;i++) {
		int sq = newline[i];
		sq_lines[sq][num_sq_lines[sq]] = num_lines;
		num_sq_lines[sq]++;		

		if (num_sq_lines[sq] == MAX_LINES_PER_SQUARE) {
			printf("MAX_LINES_PER_SQUARE exceeded (%d)\n",
			       num_sq_lines[sq]);
			exit(1);
		}
	}

	memcpy(lines[num_lines],newline,sizeof(newline));

	num_lines++;
	if (num_lines == MAX_LINES) {
		printf("MAX_LINES exceeded (%d)\n",num_lines);
		exit(1);
	}
}


static void init_lines(void)
{
	int i, x, y, z;
	memset(num_sq_lines, 0, sizeof(num_sq_lines));

	for (z=0;z<N;z++) {
		for (i=0;i<N;i++) {
			add_line(i,0,z, 0,1,0);
			add_line(0,i,z, 1,0,0);
		}
		add_line(0,0,z, 1,1,0);
		add_line(N-1,0,z,-1,1,0);
	}

	for (x=0;x<N;x++) {
		for (i=0;i<N;i++) {
			add_line(x,0,i, 0,1,0);
			add_line(x,i,0, 0,0,1);
		}
		add_line(x,0,0, 0,1,1);
		add_line(x,0,N-1, 0,1,-1);
	}

	for (y=0;y<N;y++) {
		for (i=0;i<N;i++) {
			add_line(i,y,0, 0,0,1);
			add_line(0,y,i, 1,0,0);
		}
		add_line(0,y,0, 1,0,1);
		add_line(N-1,y,0,-1,0,1);
	}

	add_line(0,0,0,  1, 1, 1);
	add_line(N-1,0,0, -1, 1, 1);
	add_line(0,N-1,0,  1,-1, 1);
	add_line(0,0,N-1,  1, 1,-1);

	printf("added %d lines\n",num_lines);
}


static struct {
	int A, B;
} line_counts[MAX_LINES];

static struct {
	int A, B;
} length_counts[N+1];

static void play_move(char *b, int move, int player)
{
	int i, l;

	b[move] = player;

	for (i=0;i<num_sq_lines[move];i++) {
		l = sq_lines[move][i];

		if (!line_counts[l].A && !line_counts[l].B) {
			if (player == PIECE_A)
				length_counts[1].A++;
			else
				length_counts[1].B++;
		} else if (line_counts[l].A && !line_counts[l].B) {
			length_counts[line_counts[l].A].A--;
			if (player == PIECE_A) {
				length_counts[line_counts[l].A+1].A++;
			}
		} else if (line_counts[l].B && !line_counts[l].A) {
			length_counts[line_counts[l].B].B--;
			if (player == PIECE_B) {
				length_counts[line_counts[l].B+1].B++;
			}
		}

		if (player == PIECE_A)
			line_counts[l].A++;
		else 
			line_counts[l].B++;
	}
}

static void unplay_move(char *b, int move, int player)
{
	int i, l;

	b[move] = 0;

	for (i=0;i<num_sq_lines[move];i++) {
		l = sq_lines[move][i];

		if (line_counts[l].A && line_counts[l].B) {
			if (line_counts[l].A == 1 && player == PIECE_A) {
				length_counts[line_counts[l].B].B++;
			} else if (line_counts[l].B == 1 && player == PIECE_B) {
				length_counts[line_counts[l].A].A++;
			}
		} else if (line_counts[l].A) {
			length_counts[line_counts[l].A].A--;
			if (line_counts[l].A > 1)
				length_counts[line_counts[l].A-1].A++;
		} else if (line_counts[l].B) {
			length_counts[line_counts[l].B].B--;
			if (line_counts[l].B > 1)
				length_counts[line_counts[l].B-1].B++;
		}

		if (player == PIECE_A)
			line_counts[l].A--;
		else 
			line_counts[l].B--;
	}
}

static int eval(int next_player)
{
	int res = 0;
	int i, v;

	if (length_counts[N].A) return +WIN;
	if (length_counts[N].B) return -WIN;

	if (next_player == PIECE_A &&
	    length_counts[N-1].A) return WIN;

	if (next_player == PIECE_B &&
	    length_counts[N-1].B) return -WIN;

	for (i=1;i<N;i++) {
		if (length_counts[i].A) {
			v = (1<<2*i) * length_counts[i].A;
			if (next_player == PIECE_A)
				v <<= 1;
			res += v;
		}

		if (length_counts[i].B) {
			v = (1<<2*i) * length_counts[i].B;
			if (next_player == PIECE_B)
				v <<= 1;
			res -= v;
		}
	}

	return res;
}

static void init_counts(char *b)
{
	int i, l, line;

	if (!num_lines)
		init_lines();

	memset(line_counts, 0, sizeof(line_counts));
	memset(length_counts, 0, sizeof(length_counts));

	for (i=0; i<NUM_SQUARES; i++)
		if (b[i])
			for (l=0;l<num_sq_lines[i];l++) {
				line = sq_lines[i][l];

				if (b[i] == PIECE_A)
					line_counts[line].A++;
				else 
					line_counts[line].B++;
			}
				
	for (l=0;l<num_lines;l++) {
		if ((line_counts[l].A && line_counts[l].B) ||
		    (!line_counts[l].A && !line_counts[l].B))
			continue;
		if (line_counts[l].A)
			length_counts[line_counts[l].A].A++;
		else length_counts[line_counts[l].B].B++;
	}
}


void find_win(char board[N][N][N], 
	      int *x, int *y, int *z, 
	      int *dx, int *dy, int *dz)
{
	int i;
	int x2, y2, z2;

	char *b = &board[0][0][0];

	init_counts(b);

	for (i=0;i<num_lines;i++) {
		if (line_counts[i].A == N ||
		    line_counts[i].B == N) {
			break;
		}
	}

	if (i==num_lines) {
		printf("no win?\n");
		return;
	}

	map1dto3d(lines[i][0], x, y, z);
	map1dto3d(lines[i][1], &x2, &y2, &z2);
	*dx = x2 - *x;
	*dy = y2 - *y;
	*dz = z2 - *z;
}


int make_move(char board[N][N][N], int player)
{
	char *b = &board[0][0][0];
	int i, v, bestv;
	int besti=-1;

	init_counts(b);

	if (length_counts[N].A) {
		printf("player A has won\n");
		return PIECE_A;
	}

	if (length_counts[N].B) {
		printf("player B has won\n");
		return PIECE_B;
	}

	for (i=0;i<NUM_SQUARES;i++)
		if (!b[i]) {
			play_move(b, i, player);
			v = eval(-player) * player;
			unplay_move(b, i, player);
			if (besti == -1 || v >= bestv) {
				if (v != bestv || ((unsigned)random()) % 2) {
					besti = i;
					bestv = v;
				}
			}
		}

	if (besti == -1) {
		printf("game is drawn\n");
		return 0;
	}

	play_move(b, besti, player);
	printf("eval %d\n",eval(-player));

	if (length_counts[N].A) {
		printf("player A has won\n");
		return PIECE_A;
	}

	if (length_counts[N].B) {
		printf("player B has won\n");
		return PIECE_B;
	}

	return 0;
}


void next_line(char board[N][N][N])
{
	static int l;
	int i;

	char *b = &board[0][0][0];

	if (!num_lines)
		init_lines();

	for (i=0;i<N;i++)
		b[lines[l][i]] = 1;

	l = (l+1)%num_lines;
}

