CC = gcc
CFLAGS = -Wall -O3
LIBS = -lzmq -lncurses -lprotobuf-c -lpthread

# Define .PHONY to specify that 'all' and 'clean' are not files.
.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: lizard_client 

# Rule for building lizard_client
lizard_client: lizard-client.c ../lizards.pb-c.c auxiliar.h  lizards.pb-c.o 
	$(CC) $(CFLAGS) -o lizard_client ../lizards.pb-c.c lizard-client.c $(LIBS)

# Rule for compiling lizards.pb-c.c to an object file
lizards.pb-c.o: ../lizards.pb-c.c ../lizards.pb-c.h
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for cleaning up the build artifacts
clean:
	rm -f lizard_client lizards.pb-c.o
