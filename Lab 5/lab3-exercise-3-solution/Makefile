CC = gcc
CFLAGS = -Wall -g
LIBS = -lzmq -lncurses

all: server human_client machine_client remote-display-client

server: server-exercise-3.c zhelpers.h remote-char.h
	$(CC) $(CFLAGS) -o server server-exercise-3.c $(LIBS)

human_client: human-client.c zhelpers.h remote-char.h
	$(CC) $(CFLAGS) -o human_client human-client.c $(LIBS)

machine_client: machine-client.c zhelpers.h remote-char.h
	$(CC) $(CFLAGS) -o machine_client machine-client.c $(LIBS)

remote-display-client: remote-display-client.c zhelpers.h remote-char.h
	$(CC) $(CFLAGS) -o remote-display-client remote-display-client.c $(LIBS)

clean:
	rm -f server human_client machine_client remote-display-client
