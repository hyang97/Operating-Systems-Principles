CC = gcc
CFLAGS = -Wall -g

lab3a: lab3a.c
	$(CC) $(CFLAGS) lab3a.c -o lab3a

check:
	./test.sh
clean:
	rm -rf lab3a *.csv *.tar.gz

dist: lab3a.c README Makefile
	tar -cvzf lab3a-204584728.tar.gz lab3a.c README Makefile
