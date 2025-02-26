#compiler and flags
CC = gcc
CFLAGS = -lzmq -lncurses -g

#target executable

all: server client

server: game-server.c
	$(CC) game-server.c -o server $(CFLAGS)

client: astronaut-client.c
	$(CC) astronaut-client.c -o client $(CFLAGS)