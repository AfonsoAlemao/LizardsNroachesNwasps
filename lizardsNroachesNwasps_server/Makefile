CC = gcc
CFLAGS = -Wall -O3
LIBS = -lzmq -lncurses

# Define .PHONY to specify that 'all' and 'clean' are not files.
.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: lizardsNroachesNwasps_server

# Rule for building lizardsNroaches_server
lizardsNroachesNwasps_server: lizardsNroachesNwasps-server.c lists.o fifo.o z_helpers.h auxiliar.h lists.h fifo.h
	$(CC) $(CFLAGS) -o lizardsNroachesNwasps_server lizardsNroachesNwasps-server.c lists.o fifo.o $(LIBS)

# Rule for compiling lists.c to an object file
lists.o: lists.c lists.h
	$(CC) $(CFLAGS) -c lists.c

# Rule for compiling fifo.c to an object file
fifo.o: fifo.c fifo.h
	$(CC) $(CFLAGS) -c fifo.c

# Rule for cleaning up the build artifacts
clean:
	rm -f lizardsNroachesNwasps_server lists.o fifo.o
