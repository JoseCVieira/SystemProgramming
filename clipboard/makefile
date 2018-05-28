CFLAGS= -lpthread -Wall -Werror -g  ## -W -ansi -pedantic

clipboard: clipboard_folder
	cp bin/clipboard ../runs/
clipboard_folder: update_library
	gcc $(CFLAGS) -o bin/clipboard clipboard.c
update_library: library.so
	cp src/clipboard.h ../library/
library.so: 
	gcc src/library.c -o ../library/library.so -ldl -shared -fPIC
