CC = gcc
C_FLAGS = -Wall -g -Wno-unused
L_FLAGS = -fsanitize=address,undefined

TARGETS = election

.PHONY: all clean

all: $(TARGETS)

# Rule to build each executable from corresponding .o file
$(TARGETS): % : %.o
	$(CC) $(L_FLAGS) -o $@ $^

# Rule to compile .c into .o
%.o: %.c
	$(CC) $(C_FLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGETS)
