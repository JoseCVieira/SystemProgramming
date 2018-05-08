CFLAGS= -Wall -Werror
DIR=bin/
LIBRARY=../clipboard/src/
app_teste: app_teste.o $(LIBRARY)/library.c
	gcc  $(CFLAGS) -o $(DIR)app_teste app_teste.o $(LIBRARY)library.c  && make clean
app_teste.o: app_teste.c $(LIBRARY)library.c 
	gcc  $(CFLAGS) -c app_teste.c $(LIBRARY)library.c
clean:
	rm -f *.o
