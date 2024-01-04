CC = gcc
CFLAGS = -Wall -O3
LIBS = -lzmq -lncurses

# Define .PHONY to specify that 'all' and 'clean' are not files.
.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: wasps_client

# Rule for building roaches_client
wasps_client: wasps-client.c auxiliar.h
	$(CC) $(CFLAGS) -o wasps_client wasps-client.c $(LIBS)

# Rule for cleaning up the build artifacts
clean:
	rm -f wasps_client
