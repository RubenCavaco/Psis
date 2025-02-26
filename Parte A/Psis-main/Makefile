#compiler and flags
CC = gcc
CFLAGS = -lzmq -lncurses -g

#target executable

all: server client display

server: game-server.c
	$(CC) game-server.c common.c -o server $(CFLAGS)

client: astronaut-client.c
	$(CC) astronaut-client.c common.c -o client $(CFLAGS)

display: outer-space-display.c
	$(CC) outer-space-display.c common.c -o display $(CFLAGS)