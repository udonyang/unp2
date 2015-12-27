MAKEFLAGS=ks
CC = gcc
CFLAGS = -Idlib/src -g -O2  -std=c99\
				 -D_REENTRANT \
				 -D_POSIX_SOURCE
LIBS = -lpthread dlib/src/libdlib.a

main: dlib main.c 

dlib:
	make -C dlib/src

run:
	#./main echo
	#./main PipeTest
	./main FullDuplexPipeTest

%: %.c
	gcc $(CFLAGS) $< $(LIBS) -o $@
