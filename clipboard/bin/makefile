CFLAGS= -lpthread -Wall -Werror  ## -W -ansi -pedantic

app_teste: app_teste.o
	gcc $(CFLAGS) -o app_teste app_teste.o src/library.so && make clean
app_teste.o: app_teste.c
	gcc $(CFLAGS) -c app_teste.c src/library.so
clean:
	rm -rf *.o
mrproper: clean
	rm -rf teste
