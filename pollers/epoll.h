// epoll.h
// Scott Bronson
// 8 Mar 2007

#include "../atom.h"

// This is not an upper-limit.  How can we get more insight into
// exactly what this does?  Is it better to guess high or guess low?
#define IO_EPOLL_MAXFDS 10240


struct io_epoll_poller {
	int epfd;
};
typedef struct io_epoll_poller io_epoll_poller;


int io_epoll_init(io_epoll_poller *poller);
int io_epoll_poller_dispose(io_epoll_poller *poller);
int io_epoll_fd_check(io_epoll_poller *poller);
int io_epoll_add(io_epoll_poller *poller, io_atom *atom, int flags);
int io_epoll_set(io_epoll_poller *poller, io_atom *atom, int flags);
int io_epoll_enable(io_epoll_poller *poller, io_atom *atom, int flags);
int io_epoll_disable(io_epoll_poller *poller, io_atom *atom, int flags);
int io_epoll_remove(io_epoll_poller *poller, io_atom *atom);
int io_epoll_wait(io_epoll_poller *poller, unsigned int timeout);
int io_epoll_dispatch(struct io_poller *poller);