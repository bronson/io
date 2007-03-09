# IO Atom Makefile
# Scott Bronson
# 8 Mar 2007


COPTS=-g -Wall -Werror

all: iotest

iotest: iotest.c atom.c atom.h pollers/select.c
	$(CC) $(COPTS) iotest.c atom.c pollers/select.c -o iotest
	
clean:
	rm -f iotest
