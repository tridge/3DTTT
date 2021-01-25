/* a 3d tic tac toe program

   Andrew.Tridgell@anu.edu.au January 1997 */

#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "3dttt.h"
#include "trackball.h"

#define N (state->n)
#define halfN (0.5*N)

#define PIECE_SIZE 0.3

#define WIN_LINE_WIDTH 10

#define DEFAULT_LEVEL 2
#define DEFAULT_N 4

#define ZGAP 1.5

#define defaultW 500
#define defaultH 500

#define MOVE(x,y,z) ((x)*N*N + (y)*N + (z))
#define BOARD(x,y,z) (state->board[MOVE(x,y,z)])

enum {MENU_RESET, MENU_SUGGEST, MENU_QUIT, MENU_START,
      MENU_COMPUTER_A, MENU_COMPUTER_B, MENU_SHOW_SEARCH, MENU_DEMO,
      MENU_LIGHTING, MENU_SAVE, MENU_RESTORE, MENU_SPEED};

enum {PIECE_CUBE, PIECE_TEAPOT, PIECE_SPHERE, PIECE_EGG,
      PIECE_CONE, PIECE_TORUS};

static float a_color[4] =      {1, 0, 0};
static float b_color[4] =      {0, 0, 1};
static float suggest_color[4] =      {0, 1, 0};
static float board_color[4] =  {1, 1, 1};
static float win_color[4] =    {0, 1, 0};

static int mouse_moved;
static float downx, downy;
static int do_motion;
static int do_scaling;
static int demo_mode;
static int use_lighting = 1;
static int piece_type = PIECE_SPHERE;
static int need_redraw;

static float scalefactor = 1.0;

static float curquat[4] = {0.6, 0.0, 0.0, 0.8};

struct state *state;

static GLuint layer_list;

#define ABS(x) ((x)<0?-(x):(x))

static void make_layer_list(void)
{
	int i;

	layer_list = glGenLists(1);

	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glNewList(layer_list, GL_COMPILE);

	glBegin(GL_LINES);
	for (i=0;i<(N+1);i++) {
		glVertex2f(0, i);
		glVertex2f(N, i);

		glVertex2f(i, 0);
		glVertex2f(i, N);
	}
	glEnd();

	glEndList();
}


void do_exit(int code)
{
	if (state) {
		state->quit = 1;
	}
	exit(code);
}

static void reset_board(void)
{
	memset(state->board, 0, N*N*N*sizeof(state->board[0]));
	state->winner = 0;
	state->num_moves = 0;
	state->next_to_play = PIECE_A;
}


static void draw_layer(void)
{
	glCallList(layer_list);
}


static void draw_grid(void)
{
	int i;

        glPushMatrix();

	glColor3fv(board_color);

	glLineWidth(2.0);

	for (i=0;i<N;i++) {
		draw_layer();
		glTranslatef(0, 0, 1);
	}

        glPopMatrix();
}

static void draw_piece(int x,int y, int z, int player, float size)
{
	if (!player) return;

	glPushMatrix();
	glTranslatef(x + 0.5, y + 0.5, z);

	switch (player) {
	case PIECE_A:
		glColor3fv(a_color);
		break;
		
	case PIECE_B:
		glColor3fv(b_color);
		break;

	case PIECE_SUGGEST:
		glColor3fv(suggest_color);
		break;
	}

	switch (piece_type) {
	case PIECE_CUBE:
		glScalef(1, 1, 1/ZGAP);
		glutSolidCube(1.5*size);
		break;

	case PIECE_TEAPOT:
		glScalef(1, 1, 1/ZGAP);
		glRotatef(-270.0, 90.0, 0.0, 0.0);
		glutSolidTeapot(size);
		break;

	case PIECE_CONE:
		glutSolidCone(size, size, 10, 10);
		break;

	case PIECE_EGG:
		glutSolidSphere(size, 10, 10);
		break;

	case PIECE_SPHERE:
		glScalef(1, 1, 1/ZGAP);
		glutSolidSphere(size, 10, 10);
		break;

	case PIECE_TORUS:
		glScalef(1, 1, 1/ZGAP);
		glutSolidTorus(size/2, size, 10, 10);
		break;

	}

	glPopMatrix();	
}

static void draw_pieces(void)
{
	int x,y,z;

	for (x=0;x<N;x++)
		for (y=0;y<N;y++)
			for (z=0;z<N;z++) 
				draw_piece(x,y,z,BOARD(x,y,z),PIECE_SIZE);
}

static void draw_winner(void)
{
	if (!state->winner) return;
	

	glPushMatrix();
	glPushAttrib(GL_LINE_BIT);

	glColor3fv(win_color);

	glLineWidth(WIN_LINE_WIDTH);

	glBegin(GL_LINES);
	glVertex3f(state->winx+0.5, state->winy+0.5, state->winz);
	glVertex3f((state->winx+0.5) + state->windx*(N-1), 
		   (state->winy+0.5) + state->windy*(N-1), 
		   (state->winz+state->windz*(N-1)));
	glEnd();
	
	glPopAttrib();
	glPopMatrix();	
}


static void position_board(void)
{
	GLfloat m[4][4];

	gluPerspective(20, 1, 0.1, 100);

	glMatrixMode(GL_MODELVIEW);

	glTranslatef(0, 0, -20);

	build_rotmatrix(m, curquat);
	glMultMatrixf(&m[0][0]);

	glScalef(scalefactor, scalefactor, scalefactor * ZGAP);

	glTranslatef(-halfN, -halfN, -0.5*(N-1));
}


static void setup_lighting(void)
{
	GLfloat light_diffuse[] = {1.0, 1.0, 1.0, 0.1};
	GLfloat light_position[] = {1.0, 1.0, 2.0, 0.0};
	float bevel_mat_ambient[] ={0.3, 0.3, 0.3, 1.0};
	float bevel_mat_shininess[] =	{40.0};
	float bevel_mat_specular[] =	{0.0, 0.0, 0.0, 0.0};
	float bevel_mat_diffuse[] =	{1.0, 0.0, 0.0, 0.0};
	float lmodel_ambient[] =  {0, 0, 0, 0.1};
	float lmodel_twoside[] =  {GL_FALSE};
	float lmodel_local[] =  {GL_FALSE};

	if (!use_lighting) {
		glDisable(GL_LIGHTING);
		return;
	}

	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, lmodel_local);
	glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);

	glMaterialfv(GL_FRONT, GL_AMBIENT, bevel_mat_ambient);
	glMaterialfv(GL_FRONT, GL_SHININESS, bevel_mat_shininess);
	glMaterialfv(GL_FRONT, GL_SPECULAR, bevel_mat_specular);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, bevel_mat_diffuse);

	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	glShadeModel(GL_SMOOTH);

}

static void draw_board(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glPushMatrix();
	glEnable(GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	position_board();
	glDisable(GL_LIGHTING);
	draw_grid();
	draw_winner();
	glDisable(GL_BLEND);
	glPopMatrix();

	setup_lighting();

	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	position_board();
	draw_pieces();
        glPopMatrix();

	glutSwapBuffers();
}

static void set_cursor(int c)
{
	static int last_c;
	if (last_c == c) return;
	glutSetCursor(c);
	last_c = c;
}

static void draw_all()
{
	set_cursor(GLUT_CURSOR_WAIT);
	draw_board();
	set_cursor(GLUT_CURSOR_CROSSHAIR);
}

static void redraw(void)
{
	need_redraw = 1;
}

static void place_piece(int px, int py)
{
	int x, y, z;
	int ret;
	GLint vp[4];
	float buffer[1000*4*sizeof(float)];

	glGetIntegerv(GL_VIEWPORT, vp);

        glPushMatrix();

	for (x=0;x<N;x++)
		for (y=0;y<N;y++)
			for (z=0;z<N;z++) {
				if (BOARD(x,y,z) && 
				    BOARD(x,y,z) != PIECE_SUGGEST) continue;
				glFeedbackBuffer(1000, GL_2D, buffer);
				glRenderMode(GL_FEEDBACK);
				glInitNames();
				glPushName(~0);
				glPushMatrix();
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				gluPickMatrix(px, vp[3] - py, 1, 1, vp);
				position_board();
				glTranslatef(x, y, z);
				glRectf(0, 0, 1, 1);
				glPopMatrix();
				
				ret = glRenderMode(GL_RENDER);
				
				if (ret != 0) {
					BOARD(x,y,z) = state->next_to_play;
					state->game_record[state->num_moves++] = MOVE(x,y,z);
					redraw();
					state->next_to_play = -state->next_to_play;
					return;
				}
			}
	glPopMatrix();
}


static void mouse_func(int button, int bstate, int x, int y)
{
	if (bstate == GLUT_DOWN && button == GLUT_LEFT_BUTTON) {
		mouse_moved = 0;
		downx = x;
		downy = y;
		if(glutGetModifiers() & GLUT_ACTIVE_SHIFT) {
			do_scaling = 1;
		} else {
			do_motion = 1;
		}
	}

	if (bstate == GLUT_UP && button == GLUT_LEFT_BUTTON) {
		do_motion = 0;
		do_scaling = 0;
		if (!mouse_moved && state->next_to_play != state->computer)
			place_piece(x,y);
	}
}

static void motion_func(int x, int y)
{
	float lastquat[4];
	GLint vp[4];
	
	if (x == downx && y == downy) return;

	mouse_moved = 1;

	if (!do_motion && !do_scaling) return;

	glGetIntegerv(GL_VIEWPORT, vp);

	if (do_motion) {
		trackball(lastquat,
			  (2.0*downx - vp[2]) / vp[2],
			  (vp[3] - 2.0*downy) / vp[3],
			  (2.0*x - vp[2]) / vp[2],
			  (vp[3] - 2.0*y) / vp[3]
			  );

		add_quats(lastquat, curquat, curquat);
	}

	if (do_scaling) {
		scalefactor *= (1.0 + (((float) (downy - y)) / vp[3]));
	}

	downx = x;
	downy = y;
	redraw();
}


struct timeval tp1,tp2;

static void start_timer()
{
  gettimeofday(&tp1,NULL);
}

static double end_timer()
{
  gettimeofday(&tp2,NULL);
  return((tp2.tv_sec - tp1.tv_sec) + 
	 (tp2.tv_usec - tp1.tv_usec)*1.0e-6);
}

static void speed_test(void)
{
	float lastquat[4];
	GLint vp[4];
	int counter = 0;
	float x1 = 190, x2 = 200;
	float y1 = 200, y2 = 200;

	start_timer();


	while (end_timer() < 5) {
		glGetIntegerv(GL_VIEWPORT, vp);

		trackball(lastquat,
			  (2.0*x1 - vp[2]) / vp[2],
			  (vp[3] - 2.0*y1) / vp[3],
			  (2.0*x2 - vp[2]) / vp[2],
			  (vp[3] - 2.0*y2) / vp[3]
			  );

		add_quats(lastquat, curquat, curquat);
		draw_all();
		counter++;
	}
	printf("%4.2f fps\n", counter / end_timer());
}

static void idle_func(void)
{
	if (demo_mode) {
		state->computer = state->next_to_play;
		if (state->winner) 
			reset_board();
	}

	if (state->next_to_play == state->computer) {
		set_cursor(GLUT_CURSOR_WAIT);
		if (state->show_search) redraw();
	} else {
		set_cursor(GLUT_CURSOR_CROSSHAIR);
	}

	if (state->moved) {
		state->moved = 0;
		redraw();
	}

	if (need_redraw) {
		need_redraw = 0;
		draw_all();
	}
	usleep(1000);
}

static void move_thread(void)
{
	while (!state->quit) {
		int move = -1;

		if (state->next_to_play != state->computer) {
			usleep(1000);
			continue;
		}

		state->winner = make_move(state->board, state->next_to_play, state->play_level, &move);
		if (move != -1) {
			state->board[move] = state->next_to_play;
			state->game_record[state->num_moves++] = move;
		}
		if (state->winner) {
			find_win(state->board, 
				 &state->winx, &state->winy, &state->winz, 
				 &state->windx, &state->windy, &state->windz);
		}
		
		state->next_to_play = -state->next_to_play;
		state->moved = 1;
	}

	do_exit(0);
}

static void save_game(char *fname)
{
	int fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd == -1) {
		perror(fname);
		return;
	}
	write(fd, state, sizeof(*state));
	close(fd);
}

static void restore_game(char *fname)
{
	int fd = open(fname, O_RDONLY);
	if (fd == -1) {
		perror(fname);
		return;
	}
	read(fd, state, sizeof(*state));
	close(fd);
}

static void undo_menu(int n)
{
	int change_player;

	if (n > state->num_moves) n = state->num_moves;

	if (n <= 0) return;

	change_player = n&1;

	while (n--) {
		state->board[state->game_record[--state->num_moves]] = 0;
	}

	state->winner = 0;

	if (change_player) {
		state->next_to_play = -state->next_to_play;
	}

	redraw();
}

static void main_menu(int v)
{
	int move=-1;

	switch (v) {
	case MENU_SAVE:
		save_game("3dttt.save");
		break;

	case MENU_RESTORE:
		restore_game("3dttt.save");
		break;

	case MENU_LIGHTING:
		use_lighting = !use_lighting;
		break;

	case MENU_DEMO:
		demo_mode = !demo_mode;
		break;

	case MENU_SHOW_SEARCH:
		state->show_search = !state->show_search;
		break;

	case MENU_COMPUTER_A:
		state->computer = PIECE_A;
		break;

	case MENU_COMPUTER_B:
		state->computer = PIECE_B;
		break;

	case MENU_RESET:
		reset_board();
		break;

	case MENU_SUGGEST:
		if (state->next_to_play == state->computer) 
			break;
		set_cursor(GLUT_CURSOR_WAIT);
		make_move(state->board, state->next_to_play, state->play_level, &move);
		set_cursor(GLUT_CURSOR_CROSSHAIR);
		if (move != -1) 
			state->board[move] = PIECE_SUGGEST;
		break;

	case MENU_QUIT:
		do_exit(0);
	case MENU_SPEED:
		speed_test();;
	}

	redraw();
}

static void level_menu(int level)
{
	state->play_level = level;
}

static void computer_menu(int player)
{
	state->computer = player;
}

static void board_size_menu(int size)
{
	reset_board();
	N = size;
	make_layer_list();
	reset_board();
	redraw();
}

static void piece_menu(int piece)
{
	piece_type = piece;
	redraw();
}

static void create_menus(void)
{
	int level, comp, size, piece, undo;
	int i;

	undo = glutCreateMenu(undo_menu);
	glutAddMenuEntry("1 move", 1);
	glutAddMenuEntry("2 moves", 2);

	piece = glutCreateMenu(piece_menu);
	glutAddMenuEntry("cube", PIECE_CUBE);
	glutAddMenuEntry("sphere", PIECE_SPHERE);
	glutAddMenuEntry("egg", PIECE_EGG);
	glutAddMenuEntry("cone", PIECE_CONE);
	glutAddMenuEntry("torus", PIECE_TORUS); 
	glutAddMenuEntry("teapot", PIECE_TEAPOT);

	level = glutCreateMenu(level_menu);
	glutAddMenuEntry("beginner", 0);
	glutAddMenuEntry("normal", 1);
	glutAddMenuEntry("intermediate", 2);
	glutAddMenuEntry("advanced", 3);
	glutAddMenuEntry("master", 4);
	glutAddMenuEntry("grand master", 5);

	comp = glutCreateMenu(computer_menu);
	glutAddMenuEntry("computer plays red", PIECE_A);
	glutAddMenuEntry("computer plays blue", PIECE_B);
	glutAddMenuEntry("disable computer", 0);

	size = glutCreateMenu(board_size_menu);
	for (i=2;i<=MAX_N;i++) {
		char name[30];
		sprintf(name,"%d",i);
		glutAddMenuEntry(name,i);
	}
		

	glutCreateMenu(main_menu);
	glutAddMenuEntry("reset game", MENU_RESET);
	glutAddMenuEntry("suggest move", MENU_SUGGEST);
	glutAddSubMenu("difficulty level",level);
	glutAddSubMenu("computer",comp);
	glutAddSubMenu("board size",size);
	glutAddSubMenu("piece type",piece);
	glutAddSubMenu("undo",undo);
	glutAddMenuEntry("show search", MENU_SHOW_SEARCH);
	glutAddMenuEntry("demo mode", MENU_DEMO);
	glutAddMenuEntry("toggle lighting", MENU_LIGHTING);
	glutAddMenuEntry("quit", MENU_QUIT);
	glutAddMenuEntry("speed test", MENU_SPEED);
	glutAddMenuEntry("save game", MENU_SAVE);
	glutAddMenuEntry("restore game", MENU_RESTORE);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}


static void create_threads(void)
{
	int page_size;
	int len, shmid;

	page_size = getpagesize();
	len = sizeof(*state);
	len = (len + page_size) & ~(page_size-1);

	shmid = shmget(IPC_PRIVATE, len, SHM_R | SHM_W);
	if (shmid == -1) {
		printf("can't get shared memory\n");
		do_exit(1);
	}
	state = (struct state *)shmat(shmid, 0, 0);
	if (!state || state == (struct state *)-1) {
		printf("can't attach to shared memory\n");
		do_exit(1);
	}
	shmctl(shmid, IPC_RMID, 0);
	memset(state, 0, len);
	state->computer = PIECE_B;
	state->play_level = DEFAULT_LEVEL;

	N = DEFAULT_N;

	reset_board();

	signal(SIGCLD, SIG_IGN);

	if (fork() == 0) {
		move_thread();
		do_exit(0);
	}
}


static void give_instructions(void)
{
	printf("3D Tic-Tac-Toe\nwritten by Andrew Tridgell\n\n");
	printf("press the right mouse button for a menu\n");
	printf("press the left mouse button to play moves or rotate the board\n");
	printf("press shift+left_mouse_button to zoom\n");
}       


int main(int argc,char *argv[])
{
	srandom(time(NULL));

	create_threads();

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);

	glutCreateWindow("3D Tic-Tac-Toe");
	glutReshapeWindow(defaultW, defaultH);
	glutDisplayFunc(draw_all);
	glutMouseFunc(mouse_func);
	glutMotionFunc(motion_func);
	glutIdleFunc(idle_func);

	/* set other relevant state information */
	glEnable(GL_DEPTH_TEST);

	set_cursor(GLUT_CURSOR_CROSSHAIR);

	make_layer_list();

	reset_board();

	create_menus();

	give_instructions();

	glutMainLoop();

	return 0;
}

