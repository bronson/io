# IO Atom Makefile
# Scott Bronson
# 8 Mar 2007


COPTS=-g -Wall -Werror

# change this to select which poller is used.
DEFS=-DUSE_EPOLL

all: iotest socktest

iotest: iotest.c atom.c atom.h poller.c poller.h pollers/select.c pollers/select.h Makefile
	$(CC) $(COPTS) $(DEFS) iotest.c atom.c poller.c pollers/select.c pollers/poll.c pollers/epoll.c -o iotest
	
socktest: socktest.c atom.c atom.h poller.c poller.h socket.c socket.h pollers/select.c pollers/select.h Makefile
	$(CC) $(COPTS) $(DEFS) socktest.c atom.c poller.c socket.c pollers/select.c pollers/poll.c pollers/epoll.c -o socktest

clean:
	rm -f iotest socktest
