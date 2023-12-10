CC = gcc
CFLAGS = -Wall -O3
LIBS = -lzmq -lncurses

# Define .PHONY to specify that 'all' and 'clean' are not files.
.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: lizard_client 

# Rule for building lizard_client
lizard_client: lizard-client.c auxiliar.h
	$(CC) $(CFLAGS) -o lizard_client lizard-client.c $(LIBS)

# Rule for cleaning up the build artifacts
clean:
	rm -f lizard_client 
