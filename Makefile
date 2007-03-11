# IO Atom Makefile
# Scott Bronson
# 8 Mar 2007


COPTS=-g -Wall -Werror

all: iotest socktest

iotest: iotest.c atom.c atom.h poller.c poller.h pollers/select.c pollers/select.h
	$(CC) $(COPTS) iotest.c atom.c poller.c pollers/select.c -o iotest
	
socktest: socktest.c atom.c atom.h poller.c poller.h socket.c socket.h pollers/select.c pollers/select.h 
	$(CC) $(COPTS) socktest.c atom.c poller.c socket.c pollers/select.c -o socktest

clean:
	rm -f iotest socktest
