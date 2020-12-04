CC = gcc
AARCH64_OPTFLAGS = $(shell bash -c "($(CC) -v 2>&1 | grep 'arm64\|aarch64' >/dev/null && echo '-O3 -mcpu=native') || echo ''" )
X86_64_OPTFLAGS  = $(shell bash -c "($(CC) -v 2>&1 | grep 'x86\_64' >/dev/null && echo '-O3 -march=native') || echo ''" )
CFLAGS = -std=c99 -Wall -Wextra $(AARCH64_OPTFLAGS) $(X86_64_OPTFLAGS)

all: xd.c
	$(CC) $(CFLAGS) -o xd xd.c

clean:
	$(RM) xd.o
