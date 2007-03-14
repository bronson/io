# IO Atom Makefile
# Scott Bronson
# 8 Mar 2007


COPTS=-g -Wall -Werror

# change this to select which poller is used.
DEFS=-DUSE_EPOLL

all: testclient testserver

iotest: iotest.c atom.c atom.h poller.c poller.h pollers/select.c pollers/select.h Makefile
	$(CC) $(COPTS) $(DEFS) iotest.c atom.c poller.c pollers/select.c pollers/poll.c pollers/epoll.c -o iotest


CSRC=atom.c poller.c socket.c
CHDR=atom.h poller.h socket.h
CSRC+=pollers/select.c pollers/poll.c pollers/epoll.c
CSRC+=pollers/select.h pollers/poll.h pollers/epoll.h

all: testclient testserver

testclient: testclient.c $(CSRC) $(CHDR) Makefile
	$(CC) $(COPTS) $(DEFS) $(CSRC) testclient.c -o testclient

testserver: testserver.c $(CSRC) $(CHDR) Makefile
	$(CC) $(COPTS) $(DEFS) $(CSRC) testserver.c -o testserver


clean:
	rm -f testclient testserver iotest
