CFLAGS= -Wall -Werror
EXTRA=-lpthread
DIR=bin/
LIBRARY=../clipboard/src/
multiple_threads: library.o
	gcc $(CFLAGS) $(EXTRA) -o $(DIR)multiple_threads multiple_threads_simple_test.c library.o
library.o:
	gcc $(CFLAGS) -c $(LIBRARY)library.c
clean:
	rm -f *.o
