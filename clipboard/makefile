CFLAGS= -lpthread -Wall -Werror -g  ## -W -ansi -pedantic
all: clipboard
clipboard: clipboard.o library.so
	gcc $(CFLAGS) -o bin/clipboard clipboard.o && make clean
clipboard.o: clipboard.c
	gcc $(CFLAGS) -c clipboard.c
library.so: src/library.c
	gcc src/library.c -o bin/src/library.so -ldl -shared -fPIC
clean:
	rm -rf *.o
mrproper: clean
	rm -rf teste
