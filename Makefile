# IO Atom Makefile
# Scott Bronson
# 8 Mar 2007


COPTS=-g -Wall -Werror

all: iotest

iotest: iotest.c atom.c atom.h pollers/select.c pollers/select.h pollers/selectop.h
	$(CC) $(COPTS) iotest.c atom.c pollers/select.c -o iotest

mtest: iotest.c atom.c atom.h pollers/select.c pollers/selectop.h pollers/multi.c pollers/multi.h
	$(CC) $(COPTS) iotest.c atom.c pollers/select.c pollers/multi.c -DMULTI=1 -o mtest

clean:
	rm -f iotest mtest
