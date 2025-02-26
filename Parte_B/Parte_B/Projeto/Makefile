#compiler and flags
CC = gcc
CFLAGS = -lzmq -lncurses -lprotobuf-c -g 

#target executable

all: server client client2 display

# Generate Protobuf files
proto: score_update.proto
	protoc --c_out=. score_update.proto
	protoc --python_out=. score_update.proto

server: game-server.c
	$(CC) game-server.c score_update.pb-c.c common.c -o server $(CFLAGS)

client: astronaut-client.c
	$(CC) astronaut-client.c common.c -o client $(CFLAGS)

client2: astronaut-display-client.c
	$(CC) astronaut-display-client.c common.c -o client2 $(CFLAGS)

display: outer-space-display.c
	$(CC) outer-space-display.c common.c -o display $(CFLAGS)
