# Makefile for 3dTTT

# what C compiler? It better be Ansi-C. Use gcc if you have it.
# you may find that KnightCap is very slow if you don't use gcc
CC = gcc

# What compiler switches do you want? These ones work well with gcc
OPT = -O2 -Wall

X11 = /usr/X11R6
XLIBS = -L$(X11)/lib -lXt -lXext -lX11 -lXi
DISPLAYFLAGS = 


# you shouldn't need to edit anything below this line. Unless
# something goes wrong.

INCLUDE = $(DISPLAYFLAGS)
CFLAGS = $(OPT) $(INCLUDE) 

TARGET = 3dttt

OBJS = 3dttt.o trackball.o move.o

$(TARGET):  $(OBJS) 
	-mv $@ $@.old
	$(CC) $(LOPT) $(OBJS) -lGL -lGLU -lglut -lm -o $@

clean:
	/bin/rm -f *.o *~ $(TARGET)

