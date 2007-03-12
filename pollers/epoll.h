// epoll.h
// Scott Bronson
// 8 Mar 2007

#include <sys/epoll.h>
#include "../atom.h"

// TODO: make this dynamic
#define IO_EPOLL_MAX_EVENTS 128

struct io_epoll_poller {
	int epfd;
	int cnt_fd;
	struct epoll_event events[IO_EPOLL_MAX_EVENTS];
};
typedef struct io_epoll_poller io_epoll_poller;


int io_epoll_init(io_epoll_poller *poller);
int io_epoll_poller_dispose(io_epoll_poller *poller);
int io_epoll_fd_check(io_epoll_poller *poller);
int io_epoll_add(io_epoll_poller *poller, io_atom *atom, int flags);
int io_epoll_set(io_epoll_poller *poller, io_atom *atom, int flags);
int io_epoll_remove(io_epoll_poller *poller, io_atom *atom);
int io_epoll_wait(io_epoll_poller *poller, unsigned int timeout);
int io_epoll_dispatch(struct io_poller *poller);

