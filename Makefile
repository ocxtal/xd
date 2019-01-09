CC = gcc
CFLAGS = -O3 -Wall -march=native

all: xd.c
	$(CC) $(CFLAGS) -o xd xd.c

clean:
	$(RM) xd.o
