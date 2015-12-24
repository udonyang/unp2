MAKEFLAGS=ks
CC = gcc
CFLAGS = -I unpv22e/lib -Idlib/src -g -O2 -D_REENTRANT
LIBS = -lpthread dlib/src/libdlib.a

main: main.c

run:
	./main echo

%: %.c
	gcc $(CFLAGS) $< $(LIBS) -o $@
