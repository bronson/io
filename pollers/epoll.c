// epoll.c
// Scott Bronson
//
// Uses epoll to retrieve IO Atom events.
// TODO: VERY UNFINISHED!!

#include <stdio.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include "../atom.h"


// TODO: need to get rid of the perror functions.


int io_epoll_init(io_epoll_poller *poller)
{
	struct rlimit rl;

	if((poller->epfd = epoll_create(MAXFDS)) < 0) {
		perror("epoll_create");
		return -1;
	}

	/*
	
	TODO: eventually we're going to have to worry about the
	artificially low rlimit.  But maybe this is something we
	should set via a wrapper script or global config?
	
	if(getrlimit(RLIMIT_NOFILE, &rl) == -1) {
		perror("getrlimit");
		return -1;
	}

	printf("rlimit maxfds: soft=%ld hard=%ld\n",
			(long)rl.rlim_cur, (long)rl.rlim_max);

	rl.rlim_cur = rl.rlim_max = num_pipes * 2 + 50;
	if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
		perror("setrlimit"); 
		exit(1);
	}
	*/
	
	return 0;
}


int io_epoll_exit(io_epoll_poller *poller)
{
	return close(poller->epfd);
}

int io_epoll_fd_check(io_epoll_poller *poller)
{
	// TODO: how do I read back what FDs are being polled?
	// never mind, wait until I turn fd_check into get_next_fd
	// or something.
	return 0;
}


int io_epoll_add(io_epoll_poller *poller, io_atom *atom, int flags)
{
	int epf = 0;
	
	if(flags & IO_READ) epf |= POLLIN|POLLRDHUP;
	if(flags & IO_WRITE) epf |= POLLOUT;

	return epoll_ctl(epfd, EPOLL_CTL_ADD, fd,
			EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLERR | EPOLLHUP);
	if(err < 0) {
		return err;
	}
}


void io_epoll_set(int fd, int flags)
{
	EPOLL_CTL_MOD
}


int io_epoll_del(int fd)
{
	EPOLL_CTL_DEL
}


void io_epoll_wait(int timeout)
{
}

