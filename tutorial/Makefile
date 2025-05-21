CC      = gcc
CFLAGS  = -Wall -g -Wno-unused            # compile with warnings and debug info
LDFLAGS = -fsanitize=address,undefined   # link with sanitizers

# List of programs to build
#PROGS   = l8-1_server l8-1_client_local l8-1_client_tcp

PROGS = l8-2_server l8-2_client


# Default target builds all programs
.PHONY: all
all: $(PROGS)

# Pattern rule to compile each .c into .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Pattern rule to link each program
%: %.o
	$(CC) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f *.o $(PROGS)