MAKEFLAGS=ks
CC = gcc
CFLAGS = -Idlib/src -g -O2  -std=c99\
				 -D_REENTRANT \
				 -D_POSIX_SOURCE \
				 -D_XOPEN_SOURCE
LIBS = -lpthread dlib/src/libdlib.a

main: dlib main.c 

dlib:
	make -C dlib/src

run:
	#./main echo
	#./main PipeTest
	#./main FullDuplexPipeTest
	#./main PopenTest
	#./main FIFOTest
	#./main FIFOClientTest
	#./main FIFOMultiCliTest
	./main FIFOMultiCliTest main.c > /dev/null
	#./main MultiFIFOMultiCliTest 10 main.c > /dev/null

echo:
	./main Echo

fifosrv:
	#./main FIFOSrvTest
	./main FIFOMultiSrvTest

look:
	ps aux | grep main

kill:
	killall -9 main

%: %.c
	gcc $(CFLAGS) $< $(LIBS) -o $@
