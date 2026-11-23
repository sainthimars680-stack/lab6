CC     = gcc
CFLAGS = -Wall -Wextra -g

all: lab6_1 file_sorter

lab6_1: lab6_1.c
	$(CC) $(CFLAGS) -o $@ $<

file_sorter: file_sorter.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f lab6_1 file_sorter

.PHONY: all clean