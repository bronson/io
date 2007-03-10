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


// This is not an upper-limit.  How can we get more insight into
// exactly what this does?  Is it better to guess high or guess low?
#define MAXFDS 10000


int epfd;


void io_epoll_init()
{
	struct rlimit rl;

	if((epfd = epoll_create(MAXFDS)) < 0) {
		perror("epoll_create");
		exit(1);
	}

	if(getrlimit(RLIMIT_NOFILE, &rl) == -1) {
		perror("getrlimit"); 
		exit(1);
	}

	printf("rlimit maxfds: soft=%ld hard=%ld\n",
			(long)rl.rlim_cur, (long)rl.rlim_max);

	rl.rlim_cur = rl.rlim_max = num_pipes * 2 + 50;
	if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
		perror("setrlimit"); 
		exit(1);
	}
}


void io_epoll_exit()
{
	close(epfd);
}


int io_epoll_add()
{
	int err;

	err = epoll_ctl(epfd, EPOLL_CTL_ADD, fd,
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

