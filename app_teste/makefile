CFLAGS= -Wall -Werror
EXTRA=-lpthread
DIR=bin/
LIBRARY=../clipboard/src/
app_teste: app_teste.o $(LIBRARY)/library.c
	gcc  $(CFLAGS) -o $(DIR)app_teste app_teste.o $(LIBRARY)library.c  && make clean
app_teste.o: app_teste.c $(LIBRARY)library.c 
	gcc  $(CFLAGS) -c app_teste.c $(LIBRARY)library.c
multiple_threads: library.o
	gcc $(CFLAGS) $(EXTRA) -o $(DIR)multiple_threads multiple_threads_simple_test.c library.o
library.o:
	gcc $(CFLAGS) -c $(LIBRARY)library.c
clean:
	rm -f *.o
