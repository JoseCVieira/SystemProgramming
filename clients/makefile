CFLAGS= -Wall -Werror
EXTRA=-lpthread
LIBRARY=../library
DEST=../runs/client
multiple_threads: 
	gcc $(EXTRA) $(CFLAGS) -o $(DEST) multiple_threads_simple_test.c $(LIBRARY)/library.so
