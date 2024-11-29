CC = gcc
CFLAGS = -Wall -O3
LIBS = -lzmq -lncurses

# Define .PHONY to specify that 'all' and 'clean' are not files.
.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: roaches_client

# Rule for building roaches_client
roaches_client: roaches-client.c auxiliar.h
	$(CC) $(CFLAGS) -o roaches_client roaches-client.c $(LIBS)

# Rule for cleaning up the build artifacts
clean:
	rm -f roaches_client
