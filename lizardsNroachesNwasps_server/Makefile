CC = gcc
CFLAGS = -Wall -O3
LIBS = -lzmq -lncurses -lprotobuf-c -lpthread

# Define .PHONY to specify that 'all' and 'clean' are not files.
.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: lizardsNroachesNwasps_server

# Rule for building lizardsNroaches_server
lizardsNroachesNwasps_server: lizardsNroachesNwasps-server.c lists.o fifo.o lizards.pb-c.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Rule for compiling lizards.pb-c.c to an object file
lizards.pb-c.o: ../lizards.pb-c.c ../lizards.pb-c.h
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for compiling lists.c to an object file
lists.o: lists.c lists.h
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for compiling fifo.c to an object file
fifo.o: fifo.c fifo.h
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for cleaning up the build artifacts
clean:
	rm -f lizardsNroachesNwasps_server lists.o fifo.o lizards.pb-c.o
