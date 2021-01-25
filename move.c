#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <alloca.h>
#include "3dttt.h"

#define MAX_LINES 300
#define MAX_LINES_PER_SQUARE 20
#define NUM_SQUARES (N*N*N)
#define WIN (1000*1000)
#define MIN_WIN (WIN/2)

#define N (state->n)

#define RANDOMISE 1
#define DISASTER_PREVENTION 0

#define ALPHA_BETA 1

#if RANDOMISE
#define PICKIT() (!(((unsigned)random()) % 4))
#else
#define PICKIT() 0
#endif

static int num_lines = 0;

extern struct state *state;

typedef int Line[MAX_N];

Line lines[MAX_LINES];

static int sq_lines[MAX_SQUARES][MAX_LINES_PER_SQUARE];
static int num_sq_lines[MAX_SQUARES];

static struct line_count {
	int A, B;
	int total;
} line_counts[MAX_LINES];

static struct length_count {
	int A, B;
} length_counts[MAX_N+1];

static int nodes;
static int maxply;
static int follow_threats;

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
	static char ret[4][30];
	static unsigned next;
	char *p = ret[next];
	int x,y,z;
	next = (next+1)%4;
	map1dto3d(i, &x, &y, &z);
	sprintf(p,"(%d,%d,%d)",x,y,z);
	return p;
}

static void add_line(int x,int y,int z,int dx,int dy,int dz)
{
	int l, i;
	Line newline;

	for (i=0;i<N;i++)
		newline[i] = map3dto1d(x+dx*i,y+dy*i,z+dz*i);

	for (l=0;l<num_lines;l++)
		if (memcmp(newline, lines[l], N*sizeof(newline[0])) == 0)
			return;

	for (i=0;i<N;i++) {
		int sq = newline[i];
		sq_lines[sq][num_sq_lines[sq]] = num_lines;
		num_sq_lines[sq]++;		

		if (num_sq_lines[sq] == MAX_LINES_PER_SQUARE) {
			printf("MAX_LINES_PER_SQUARE exceeded (%d)\n",
			       num_sq_lines[sq]);
			do_exit(1);
		}
	}

	memcpy(lines[num_lines],newline,sizeof(newline));

	num_lines++;
	if (num_lines == MAX_LINES) {
		printf("MAX_LINES exceeded (%d)\n",num_lines);
		do_exit(1);
	}
}


static void init_lines(void)
{
	int i, x, y, z;
	memset(num_sq_lines, 0, sizeof(num_sq_lines));
	num_lines = 0;

	add_line(0,0,0,  1, 1, 1);
	add_line(N-1,0,0, -1, 1, 1);
	add_line(0,N-1,0,  1,-1, 1);
	add_line(0,0,N-1,  1, 1,-1);

	for (z=0;z<N;z++) {
		add_line(0,0,z, 1,1,0);
		add_line(N-1,0,z,-1,1,0);
	}

	for (x=0;x<N;x++) {
		add_line(x,0,0, 0,1,1);
		add_line(x,0,N-1, 0,1,-1);
	}

	for (y=0;y<N;y++) {
		add_line(0,y,0, 1,0,1);
		add_line(N-1,y,0,-1,0,1);
	}

	for (z=0;z<N;z++) {
		for (i=0;i<N;i++) {
			add_line(i,0,z, 0,1,0);
			add_line(0,i,z, 1,0,0);
		}
	}

	for (x=0;x<N;x++) {
		for (i=0;i<N;i++) {
			add_line(x,0,i, 0,1,0);
			add_line(x,i,0, 0,0,1);
		}
	}

	for (y=0;y<N;y++) {
		for (i=0;i<N;i++) {
			add_line(i,y,0, 0,0,1);
			add_line(0,y,i, 1,0,0);
		}
	}
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
		if (line_counts[l].A) {
			line_counts[l].total = line_counts[l].A;
			length_counts[line_counts[l].A].A++;
		} else {
			line_counts[l].total = line_counts[l].B;
			length_counts[line_counts[l].B].B++;
		}
	}
}



static void unplay_move(char *b, int move, int player)
{
	int i, l;

	b[move] = 0;

	for (i=0;i<num_sq_lines[move];i++) {
		l = sq_lines[move][i];

		if (line_counts[l].A && line_counts[l].B) {
			if (player == PIECE_A && line_counts[l].A == 1) {
				length_counts[line_counts[l].B].B++;
				line_counts[l].total = line_counts[l].B;
			} else if (player == PIECE_B && line_counts[l].B == 1) {
				length_counts[line_counts[l].A].A++;
				line_counts[l].total = line_counts[l].A;
			}
		} else if (line_counts[l].A) {
			length_counts[line_counts[l].A].A--;
			if (line_counts[l].A > 1)
				length_counts[line_counts[l].A-1].A++;
			line_counts[l].total--;
		} else if (line_counts[l].B) {
			length_counts[line_counts[l].B].B--;
			if (line_counts[l].B > 1)
				length_counts[line_counts[l].B-1].B++;
			line_counts[l].total--;
		}

		if (player == PIECE_A)
			line_counts[l].A--;
		else 
			line_counts[l].B--;
	}
}

static void play_move(char *b, int move, int player)
{
	int i, l;

	b[move] = player;

	for (i=0;i<num_sq_lines[move];i++) {
		l = sq_lines[move][i];

		if (!line_counts[l].A && !line_counts[l].B) {
			if (player == PIECE_A) {
				length_counts[1].A++;
			} else {
				length_counts[1].B++;
			}
			line_counts[l].total = 1;
		} else if (line_counts[l].A && !line_counts[l].B) {
			length_counts[line_counts[l].A].A--;
			if (player == PIECE_A) {
				length_counts[line_counts[l].A+1].A++;
				line_counts[l].total++;
			} else {
				line_counts[l].total = 0;
			}
		} else if (line_counts[l].B && !line_counts[l].A) {
			length_counts[line_counts[l].B].B--;
			if (player == PIECE_B) {
				length_counts[line_counts[l].B+1].B++;
				line_counts[l].total++;
			} else {
				line_counts[l].total = 0;
			}
		}

		if (player == PIECE_A)
			line_counts[l].A++;
		else 
			line_counts[l].B++;
	}
}

static int eval(char *b,int next_player,int ply)
{
	int res;
	int i, l, v;

	nodes++;

	if (length_counts[N].A) return +(WIN-ply);
	if (length_counts[N].B) return -(WIN-ply);

	if (next_player == PIECE_A &&
	    length_counts[N-1].A) return +(WIN-(ply+1));

	if (next_player == PIECE_B &&
	    length_counts[N-1].B) return -(WIN-(ply+1));

	if (length_counts[N-1].A > 1) return +(WIN-(ply+2));
	if (length_counts[N-1].B > 1) return -(WIN-(ply+2));

	res = 0;

	for (i=1;i<N;i++) {
		if ((l=length_counts[i].A)) {
			v = l * i;
			res += v;
		}

		if ((l=length_counts[i].B)) {
			v = l * i;
			res -= v;
		}
	}

	return res * 10;
}


typedef struct {
	int move;
	int v;
} Move;

static int move_comp(Move *m1, Move *m2)
{
	return m2->v - m1->v;
}

static int generate_moves(char *b, int player, Move *moves, int ply)
{
	int i, j, l, wa, wb;
	int n=0;

	/* when someone has a win there are no moves possible */
	if (length_counts[N].A || length_counts[N].B) return 0;

	/* if we can win in one then there is only one possible move */
	if ((length_counts[N-1].A && player == PIECE_A) ||
	    (length_counts[N-1].B && player == PIECE_B)) {
		for (l=0;l<num_lines;l++)
			if (line_counts[l].total == N-1 &&
			    ((player == PIECE_A && line_counts[l].A) ||
			     (player == PIECE_B && line_counts[l].B))) break;
		for (j=0;j<N;j++)
			if (!b[lines[l][j]]) break;
		i = lines[l][j];
		moves[0].v = eval(b, player, ply) * player;
		moves[0].move = i;
		return 1;
	}

	/* if we are threatened then there is only one possible move */
	if (length_counts[N-1].A || length_counts[N-1].B) {
		for (l=0;l<num_lines;l++)
			if (line_counts[l].total == N-1) break;
		for (j=0;j<N;j++)
			if (!b[lines[l][j]]) break;
		i = lines[l][j];
		play_move(b, i, player);
		moves[0].v = eval(b, -player, ply+1) * player;
		unplay_move(b, i, player);
		moves[0].move = i;
		return 1;
	}

	/* there might be lots of possible moves */
	for (i=0;i<NUM_SQUARES;i++) {
		if (b[i]) continue;
		moves[n].v = 0;
		moves[n].move = i;
		wa = wb = 0;
		for (j=0;j<num_sq_lines[i];j++) {
			int t = line_counts[l = sq_lines[i][j]].total + 1;
			if (t == N-1) {
				if (line_counts[l].A) wa++;
				else wb++;
			}

			/* we slightly prefer defensive moves */
			if (t>1 && line_counts[l].A && player == PIECE_B) t++;
			if (t>1 && line_counts[l].B && player == PIECE_A) t++;

			if (t == 1 && line_counts[l].A)
				t = 0;
			moves[n].v += t * t;
		}
		if ((wa > 1 && player == PIECE_A) || 
		    (wb > 1 && player == PIECE_B)) {
			/* we've found a way to get two lots of 
			   N-1 in a row - a winner! */
			play_move(b, i, player);
			moves[0].v = eval(b, -player, ply+1) * player;
			unplay_move(b, i, player);
			moves[0].move = i;
			return 1;
		}
		n++;
	}

	qsort(moves, n, sizeof(moves[0]), move_comp); 
	
	return n;
}

static int find_move(char *b, int player, int depth, int *outv, int besta, int bestb, int ply, int veto)
{
	int i, besti, bestm, bestv, v, m;
	Move *moves;
	int num_moves;

	if (ply > maxply) maxply = ply;

	moves = (Move *)alloca(sizeof(moves[0])*NUM_SQUARES);
	num_moves = generate_moves(b, player, moves, ply);

	besti = -1;
	bestm = -1;
	bestv = -(2 * WIN);

	if (num_moves == 0) {
		(*outv) = 0;
		return -1;
	}

	if (num_moves == 1) {
		besti = moves[0].move;
		if (follow_threats &&
		    moves[0].v > -MIN_WIN && moves[0].v < MIN_WIN) {
			play_move(b, besti, player);
			find_move(b, -player, depth, outv, besta, bestb, ply+1, -1);
			unplay_move(b, besti, player);
		} else {
			(*outv) = moves[0].v * player;
		}
		return besti;
	}

	for (m=0;m<num_moves;m++) {
		i = moves[m].move;
		v = moves[m].v;

		if (i == veto) continue;

		play_move(b, i, player);

		if (depth) {
			find_move(b, -player, depth-1, &v, besta, bestb, ply+1, -1);
			v *= player;
		} else {
			v = eval(b, -player, ply+1) * player;
		}

		unplay_move(b, i, player);

#if 0
		if (ply == 1)
			printf("%s v=%d bestv=%d besti=%s moves[m].v=%d\n",
			       posstr(i), v, bestv, posstr(besti), moves[m].v);
#endif
	
		if (v > bestv || 
		    (v == bestv && moves[m].v > moves[bestm].v) ||
		    (v == bestv && moves[m].v == moves[bestm].v && PICKIT())) {
			besti = i;
			bestm = m;
			bestv = v;
		}
#if ALPHA_BETA
		if ((player == PIECE_A && bestv >= -bestb) ||
		    (player == PIECE_B && bestv >= -besta))
			goto finished;
		if (player == PIECE_A && bestv > besta) besta = bestv;
		if (player == PIECE_B && bestv > bestb) bestb = bestv;
#endif
	}

finished:

	if (besti == -1)
		bestv = 0;

	(*outv) = bestv * player;
	return besti;
}

char b_alt[MAX_SQUARES];

int make_move(char *b_orig, int player, int play_level, int *move)
{
	int besti = -1, bestv, depth, max_depth;
	int min_nodes = 500;
	int i;
	static int lastN;
	char *b = b_orig;

	for (i=0;i<NUM_SQUARES;i++) {
		if (b[i] == PIECE_SUGGEST) b[i] = 0;
	}

	if (!state->show_search) {
		memcpy(b_alt, b_orig, NUM_SQUARES*sizeof(b[0]));
		b = b_alt;
	} else {
		b = b_orig;
	}

	if (N != lastN) {
		init_lines();
		lastN = N;
	}

	*move = -1;

	init_counts(b);

	if (play_level == 0)
		follow_threats = 0;
	else
		follow_threats = 1;
		
	max_depth = play_level;
	if (play_level > 2)
		max_depth = NUM_SQUARES;

	for (i=0; i<play_level; i++)
		min_nodes *= 5;

	depth = 0;
	nodes = 0;
	maxply = 0;
	
	if (length_counts[N].A) {
		printf("player A has won\n");
		return PIECE_A;
	}

	if (length_counts[N].B) {
		printf("player B has won\n");
		return PIECE_B;
	}

	if (length_counts[N-1].A || length_counts[N-1].B) {
		max_depth = depth = 0;
	}

again:
	besti = find_move(b, player, depth, &bestv, -2*WIN, -2*WIN, 1, -1);

	if (besti == -1) {
		printf("game is drawn\n");
		return 0;
	}

	if (bestv < MIN_WIN && bestv > -MIN_WIN &&
	    nodes < min_nodes && depth < max_depth) {
		depth++;
		goto again;
	}

#if 0
	/* if we didn't find a win then we may be better off just using
	   level 0 heuristics */
	if (bestv < MIN_WIN && bestv > -MIN_WIN && depth) {
		depth = max_depth = 0;
		goto again;
	}
#endif

	play_move(b, besti, player);

#if DISASTER_PREVENTION
	/* if we think this is a moderate move then check by finding
	   the opponents best reply - if the best reply is really good then
	   abandon the move */
	if (bestv < MIN_WIN && bestv > -MIN_WIN) {
		find_move(b, -player, depth, &bestv, -2*WIN, -2*WIN, 2, -1);
		if (bestv * player < -MIN_WIN) {
			/* yikes - its a disaster, try another */
			printf("disaster prevention ...\n");
			unplay_move(b, besti, player);
			besti = find_move(b, player, depth, &bestv, -2*WIN, -2*WIN, 1, besti);
			play_move(b, besti, player);
		} else if (bestv * player > MIN_WIN) {
			printf("move improved\n");
		}

	}
#endif

	*move = besti;

	if (b != b_orig) {
		memcpy(b_orig, b, NUM_SQUARES*sizeof(b[0]));
	}

	if (length_counts[N].A) {
		printf("player A has won\n");
		return PIECE_A;
	}

	if (length_counts[N].B) {
		printf("player B has won\n");
		return PIECE_B;
	}

	printf("%s: ",player==PIECE_A?"A":"B");

	printf("eval %d nodes  %d:%d ply ",nodes, depth, maxply);

	if (depth)
		printf(" -> %d ", bestv);

	if (length_counts[N-1].A && player == PIECE_A)
		printf("(threat A) ");

	if (length_counts[N-1].B && player == PIECE_B)
		printf("(threat B) ");

	if (bestv * player >= MIN_WIN)
		printf("(winning) ");

	if (bestv * player <= -MIN_WIN)
		printf("(losing) ");

	if (besti == -1)
		printf("(drawn) ");

	printf("\n");

	return 0;
}


void find_win(char *b,
	      int *x, int *y, int *z, 
	      int *dx, int *dy, int *dz)
{
	int i;
	int x2, y2, z2;

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

