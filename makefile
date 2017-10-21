CC=gcc
CFLAGS= -Wall

all: http-server

http-server:
	$(CC) ${CFLAGS} http-server.c -o http-server

clean:
	rm -Rf http-server
