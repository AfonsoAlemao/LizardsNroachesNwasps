CC = gcc
CFLAGS = -Wall -g
LIBS = -lzmq -lncurses

# Define .PHONY to specify that 'all' and 'clean' are not files.
.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: lizardsNroaches_server roaches_client lizard_client remote_display_client

# Rule for building lizardsNroaches_server
lizardsNroaches_server: lizardsNroaches-server.c lists.o fifo.o zhelpers.h remote-char.h lists.h fifo.h
	$(CC) $(CFLAGS) -o lizardsNroaches_server lizardsNroaches-server.c lists.o fifo.o $(LIBS)

# Rule for compiling lists.c to an object file
lists.o: lists.c lists.h
	$(CC) $(CFLAGS) -c lists.c

# Rule for compiling fifo.c to an object file
fifo.o: fifo.c fifo.h
	$(CC) $(CFLAGS) -c fifo.c

# Rule for building roaches_client
roaches_client: roaches-client.c zhelpers.h remote-char.h
	$(CC) $(CFLAGS) -o roaches_client roaches-client.c $(LIBS)

# Rule for building lizard_client
lizard_client: lizard-client.c zhelpers.h remote-char.h
	$(CC) $(CFLAGS) -o lizard_client lizard-client.c $(LIBS)

# Rule for building lizard_client
remote_display_client: remote-display-client.c zhelpers.h remote-char.h
	$(CC) $(CFLAGS) -o remote_display_client remote-display-client.c $(LIBS)

# Rule for cleaning up the build artifacts
clean:
	rm -f lizardsNroaches_server roaches_client lizard_client remote_display_client lists.o fifo.o
