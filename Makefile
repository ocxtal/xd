CC = gcc
CFLAGS = -std=c99 -O3 -Wall -march=native

all: xd.c
	$(CC) $(CFLAGS) -o xd xd.c

clean:
	$(RM) xd.o
