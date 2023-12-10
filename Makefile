CC = gcc
CFLAGS = -Wall -O3
LIBS = -lzmq -lncurses

.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: lizardsNroaches-server/lizardsNroaches_server roaches-client/roaches_client lizard-client/lizard_client display-app/display_app

# Rule for building lizardsNroaches_server
lizardsNroaches-server/lizardsNroaches_server: lizardsNroaches-server/lizardsNroaches-server.c lists.o fifo.o z_helpers.h auxiliar.h lists.h fifo.h
	$(CC) $(CFLAGS) -o $@ lizardsNroaches-server/lizardsNroaches-server.c lists.o fifo.o -Iinclude $(LIBS) -I.

# Rule for compiling lists.c to an object file
lists.o: lists.c lists.h
	$(CC) $(CFLAGS) -c $<

# Rule for compiling fifo.c to an object file
fifo.o: fifo.c fifo.h
	$(CC) $(CFLAGS) -c $<

# Rule for building roaches_client
roaches-client/roaches_client: roaches-client/roaches-client.c auxiliar.h
	$(CC) $(CFLAGS) -o $@ roaches-client/roaches-client.c -Iinclude $(LIBS) -I.

# Rule for building lizard_client
lizard-client/lizard_client: lizard-client/lizard-client.c z_helpers.h auxiliar.h
	$(CC) $(CFLAGS) -o $@ lizard-client/lizard-client.c -Iinclude -I. $(LIBS)

# Rule for display-app
display-app/display_app: display-app/display-app.c z_helpers2.h auxiliar.h
	$(CC) $(CFLAGS) -o $@ display-app/display-app.c -Iinclude -I. $(LIBS) -L/path/to/ncurses/library/directory -lncurses


# Rule for cleaning up the build artifacts
clean:
	rm -f lizardsNroaches-server/lizardsNroaches_server roaches-client/roaches_client lizard-client/lizard_client display-app/display_app lists.o fifo.o
