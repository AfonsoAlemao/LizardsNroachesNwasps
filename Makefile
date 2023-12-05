CC = gcc
CFLAGS = -Wall -g
LIBS = -lzmq -lncurses

# Define .PHONY to specify that 'all' and 'clean' are not files.
.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: lizardsNroaches_server roaches_client

# Rule for building lizardsNroaches_server
lizardsNroaches_server: lizardsNroaches-server.c zhelpers.h remote-char.h
	$(CC) $(CFLAGS) -o lizardsNroaches_server lizardsNroaches-server.c $(LIBS)

# Rule for building roaches_client
roaches_client: roaches-client.c zhelpers.h remote-char.h
	$(CC) $(CFLAGS) -o roaches_client roaches-client.c $(LIBS)

# Rule for cleaning up the build artifacts
clean:
	rm -f lizardsNroaches_server roaches_client
