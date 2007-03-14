// epoll.c
// Scott Bronson
// 8 May 2007
//
// Uses epoll to retrieve IO Atom events.


#include <unistd.h>
#include <errno.h>
#include <values.h>

#include "../poller.h"


int io_epoll_init(io_epoll_poller *poller)
{
	// struct rlimit rl;

	// TODO: allow user to supply the hint instead of
	// hard-coding 1024!
	poller->epfd = epoll_create(1024);
	if(poller->epfd < 0) {
		return poller->epfd;
	}

	/*
	
	TODO: eventually we're going to have to worry about
	artificially low rlimits.  But maybe this is something
	we should set via a wrapper script or global config?
	
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


int io_epoll_poller_dispose(io_epoll_poller *poller)
{
	return close(poller->epfd);
}


int io_epoll_fd_check(io_epoll_poller *poller)
{
	// TODO: how do I read back what FDs are being polled?
	// need to wait until I turn fd_check into get_next_fd
	// or something.
	return 0;
}


static int get_events(int flags)
{
	int events = 0;
	// EPOLLPRI -- (out only) urgent data is available (?)
	// EPOLLMSG -- ??
	// EPOLLERR -- (out only) error on the file descriptor
	// EPOLLHUP -- (out only) remote has closed its connection.
	// EPOLLONESHOT -- (in only?) must re-arm the event each time you recieve it.
	// EPOLLET -- (in only?) want operations to be edge-triggered
	if(flags & IO_READ) events |= EPOLLIN|EPOLLET;
	if(flags & IO_WRITE) events |= EPOLLOUT|EPOLLET;

	return events;
}


int io_epoll_add(io_epoll_poller *poller, io_atom *atom, int flags)
{
	struct epoll_event event;
	event.data.ptr = atom;
	event.events = get_events(flags);
	return epoll_ctl(poller->epfd, EPOLL_CTL_ADD, atom->fd, &event);
}


int io_epoll_set(io_epoll_poller *poller, io_atom *atom, int flags)
{
	struct epoll_event event;
	event.data.ptr = atom;
	event.events = get_events(flags);
	return epoll_ctl(poller->epfd, EPOLL_CTL_MOD, atom->fd, &event);
}


int io_epoll_remove(io_epoll_poller *poller, io_atom *atom)
{
	// Linux has been able to handle a NULL event only since 2.6.9.
	struct epoll_event event;
	return epoll_ctl(poller->epfd, EPOLL_CTL_DEL, atom->fd, &event);
}


int io_epoll_wait(io_epoll_poller *poller, unsigned int timeout)
{
	int to;
	
	if(timeout >= INT_MAX) {
		to = -1;
	} else {
		to = timeout;
	}
	
	poller->cnt_fd = epoll_wait(poller->epfd, poller->events, IO_EPOLL_MAX_EVENTS, to);
    if(poller->cnt_fd < 0) {
        // it's not an error if we were interrupted.
        if(errno == EINTR) {
            poller->cnt_fd = 0;
        }
    }

	return poller->cnt_fd;
}


int io_epoll_dispatch(struct io_poller *base_poller)
{
	int i, max, events;
	io_atom *atom;
	io_epoll_poller *poller = &base_poller->poller_data.epoll;
	    
    max = poller->cnt_fd;
    for(i=0; i < max; i++) {
    	atom = (io_atom*)poller->events[i].data.ptr;
    	events = poller->events[i].events;
    	if(events & EPOLLIN) {
    		(*atom->read_proc)(base_poller, atom);
    	}
    	if(events & EPOLLOUT) {
    		(*atom->write_proc)(base_poller, atom);
    	}
    }
    
    return 0;
}

