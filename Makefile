# IO Atom Makefile
# Scott Bronson
# 8 Mar 2007


COPTS=-g -Wall -Werror

all: iotest

iotest: iotest.c atom.c atom.h poll/select.c
	$(CC) $(COPTS) iotest.c atom.c poll/select.c -o iotest
	
clean:
	rm -f iotest