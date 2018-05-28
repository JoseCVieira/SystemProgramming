CFLAGS= -lpthread -Wall -Werror -g  ## -W -ansi -pedantic

clipboard: clipboard_folder
	cp bin/clipboard ../testes/
clipboard_folder: library.so
	gcc $(CFLAGS) -o bin/clipboard clipboard.c
library.so: src/library.c
	gcc src/library.c -o ../library/library.so -ldl -shared -fPIC
