CFLAGS= -Wall -Werror
EXTRA=-lpthread
LIBRARY=src/
app_teste: app_teste.o $(LIBRARY)/library.so
	gcc  $(CFLAGS) -o app_teste app_teste.o $(LIBRARY)library.so && make clean
app_teste.o: app_teste.c $(LIBRARY)library.so
	gcc  $(CFLAGS) -c app_teste.c $(LIBRARY)library.so
clean:
	rm -f *.o
