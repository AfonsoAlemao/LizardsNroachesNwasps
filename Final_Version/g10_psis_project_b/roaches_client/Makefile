CC = gcc
CFLAGS = -Wall -O3
LIBS = -lzmq -lncurses -lprotobuf-c

# Define .PHONY to specify that 'all' and 'clean' are not files.
.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: roaches_client

# Rule for building roaches_client
roaches_client: roaches-client.c lizards.pb-c.o auxiliar.h
	$(CC) $(CFLAGS) -o $@ roaches-client.c lizards.pb-c.o $(LIBS)

# Rule for compiling lizards.pb-c.c to an object file
lizards.pb-c.o: ../lizards.pb-c.c ../lizards.pb-c.h
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for cleaning up the build artifacts
clean:
	rm -f roaches_client lizards.pb-c.o
 