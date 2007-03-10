// poll.c
// Scott Bronson
//
// Uses poll to satisfy gatekeeper's network I/O

#include <sys/poll.h>
#include "../atom.h"


#define MAXFDS 

struct pollfd ufds[MAXFDS];


void io_poll_init()
{
}


void io_poll_exit()
{
	close(epfd);
}


void io_poll_wait()
{
	int nfds;

	nfds = epoll_wait(epfd, events, maxevent
}

