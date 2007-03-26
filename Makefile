# IO Atom Makefile
# Scott Bronson
# 8 Mar 2007


COPTS=-g -Wall -Werror

# change this to select which poller is used.
DEFS=-DUSE_EPOLL -DUSE_MOCK

all: testclient testserver

iotest: iotest.c atom.c atom.h poller.c poller.h socket.c socket.h pollers/select.c pollers/select.h Makefile
	$(CC) $(COPTS) $(DEFS) iotest.c atom.c poller.c socket.c pollers/select.c pollers/poll.c pollers/epoll.c -o iotest


CSRC=atom.c poller.c socket.c
CHDR=atom.h poller.h socket.h
CSRC+=pollers/select.c pollers/poll.c pollers/epoll.c pollers/mock.c
CSRC+=pollers/select.h pollers/poll.h pollers/epoll.h pollers/mock.h

all: testclient testserver

testclient: testclient.c $(CSRC) $(CHDR) Makefile
	$(CC) $(COPTS) $(DEFS) $(CSRC) testclient.c -o testclient

testserver: testserver.c $(CSRC) $(CHDR) Makefile
	$(CC) $(COPTS) $(DEFS) $(CSRC) testserver.c -o testserver

testmock: testmock.c $(CSRC) $(CHDR) Makefile
	$(CC) $(COPTS) $(DEFS) $(CSRC) testmock.c -o testmock

clean:
	rm -f testclient testserver iotest
