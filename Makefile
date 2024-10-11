# Define compiler and compiler flags
CC = gcc
CFLAGS = -Wall -g

# Define target executable names
SENDFILE = sendfile
RECVFILE = recvfile

# Define source files
SENDFILE_SRC = sendfile.c
RECVFILE_SRC = recvfile.c

# Default target: Compile both sendfile and recvfile
all: $(SENDFILE) $(RECVFILE)

# Rule to compile sendfile
$(SENDFILE): $(SENDFILE_SRC)
	$(CC) $(CFLAGS) -o $(SENDFILE) $(SENDFILE_SRC)

# Rule to compile recvfile
$(RECVFILE): $(RECVFILE_SRC)
	$(CC) $(CFLAGS) -o $(RECVFILE) $(RECVFILE_SRC)

# Clean up compiled files
clean:
	rm -f $(SENDFILE) $(RECVFILE)

# Phony targets to avoid conflicts with files of the same name
.PHONY: all clean
