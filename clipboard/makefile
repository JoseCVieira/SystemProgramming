CFLAGS= -lpthread -Wall -Werror
DIR=bin/

clipboard: clipboard.o 
	gcc $(CFLAGS) -o $(DIR)clipboard clipboard.o && make clean

clipboard.o: clipboard.c 
	gcc $(CFLAGS) -c clipboard.c

clean:
	rm -f *.o
