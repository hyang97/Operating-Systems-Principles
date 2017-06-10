all: client server

client: client.c
	gcc -g -lmcrypt -pthread client.c -o client

server: server.c
	gcc -g -lmcrypt -pthread server.c -o server

clean:
	rm -f client server *.txt 

dist:
	tar cvzf lab1b-204584728.tar.gz client.c server.c Makefile my.key README
