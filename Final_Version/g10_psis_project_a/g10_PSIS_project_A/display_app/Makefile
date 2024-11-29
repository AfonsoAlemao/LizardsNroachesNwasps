CC = gcc
CFLAGS = -Wall -O3
LIBS = -lzmq -lncurses

# Define .PHONY to specify that 'all' and 'clean' are not files.
.PHONY: all clean

# 'all' is the default target. It depends on the binaries that need to be built.
all: display_app

# Rule for display-app
display_app: display-app.c z_helpers2.h auxiliar.h
	$(CC) $(CFLAGS) -o display_app display-app.c $(LIBS)

# Rule for cleaning up the build artifacts
clean:
	rm -f display_app 
