// poll.h
// Scott Bronson
// 7 Mar 2007

#include <sys/poll.h>
#include "../atom.h"


#ifndef IO_POLL_MAX_FDS
#define IO_POLL_MAX_FDS 1024
#endif


struct io_poll_poller {
	int num_pfds;	// the index plus one of the last pfd in the array.
	int cnt_fd;		// the number of fds that have events on them (set by io_poll_wait()).	
	io_atom* connections[IO_POLL_MAX_FDS];
	struct pollfd pfds[IO_POLL_MAX_FDS];      
};
typedef struct io_poll_poller io_poll_poller;


int io_poll_init(io_poll_poller *poller);
int io_poll_poller_dispose(io_poll_poller *poller);
int io_poll_fd_check(io_poll_poller *poller);
int io_poll_add(io_poll_poller *poller, io_atom *atom, int flags);
int io_poll_set(io_poll_poller *poller, io_atom *atom, int flags);
int io_poll_enable(io_poll_poller *poller, io_atom *atom, int flags);
int io_poll_disable(io_poll_poller *poller, io_atom *atom, int flags);
int io_poll_remove(io_poll_poller *poller, io_atom *atom);
int io_poll_wait(io_poll_poller *poller, unsigned int timeout);
int io_poll_dispatch(struct io_poller *poller);

