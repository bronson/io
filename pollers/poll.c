// poll.c
// Scott Bronson
//
// Uses poll to satisfy gatekeeper's network I/O

// TODO: could adding POLLRDHUP, POLLERR, or POLLHUP help?

#ifdef USE_POLL

#include <stddef.h>
#include <errno.h>
#include <sys/poll.h>
#include <values.h>

#include "../poller.h"


int io_poll_init(io_poll_poller *poller)
{
	// TODO: right now max_fd only goes up in value.
	// we should add some better heuristics/ordering so we don't have to
	// do a full linear search every time someone calls io_add,
	// and so max_fd can decrease when the number of simultaneous
	// connections reduces.
	poller->num_pfds = 0;
	return 0;
}


int io_poll_poller_dispose(io_poll_poller *poller)
{
	// nothing to dispose of
	return 0;
}


int io_poll_fd_check(io_poll_poller *poller)
{
	// TODO: unimplemented!
	return -1;
}


// Converts from IO Atom's flags to poll(2)'s flags.

static int get_events(int flags)
{
	int events = 0;
	
	if(flags & IO_READ) events |= POLLIN;
	if(flags & IO_WRITE) events |= POLLOUT;

	return events;
}


// Scans through the set of polled fds looking for the given fd.
// If avail points to an int, then the first available slot is
// returned.  If no slots are available, *avail is set to -1.

static int find_fd(io_poll_poller *poller, int fd, int *avail)
{
	int i, max;
	
	if(avail) {
		*avail = -1;
	}
	
	for(i=0, max=poller->num_pfds; i<max; i++) {
		if(poller->pfds[i].fd == fd) {
			return i;
		}
		if(avail && poller->pfds[i].fd == -1) {
			*avail = i;
			avail = NULL;
		}
	}
	
	return -1;
}


int io_poll_add(io_poll_poller *poller, io_atom *atom, int flags)
{
	int index, avail_fd;
	
	if(atom->fd < 0) {
		return -ERANGE;
	}
	
	index = find_fd(poller, atom->fd, &avail_fd);
	if(index >= 0) {
		// this fd is already being monitored!
		return -EALREADY;
	}
	
	if(avail_fd == -1) {
		// if no available fds, we need to allocate another one
		if(poller->num_pfds > IO_POLL_MAX_FDS) {
			return -EMFILE;
		}
		index = poller->num_pfds;
		poller->num_pfds += 1;
	}
		
	poller->pfds[index].fd = atom->fd;
	poller->pfds[index].events = get_events(flags);
	poller->connections[index] = atom;
	
	return 0;
}


static int poll_find(io_poll_poller *poller, int fd)
{	
	int index;
	
	if(fd < 0) {
		return -ERANGE;
	}
	
	index = find_fd(poller, fd, NULL);
	if(index < 0) {
		return -EEXIST;
	}
	
	return index;
}


int io_poll_set(io_poll_poller *poller, io_atom *atom, int flags)
{
	int index = poll_find(poller, atom->fd);
	if(index < 0) {
		return index;
	}
	
	poller->pfds[index].events = get_events(flags);
	return 0;
}


int io_poll_remove(io_poll_poller *poller, io_atom *atom)
{
	int index = poll_find(poller, atom->fd);
	if(index < 0) {
		return index;
	}
	
	poller->pfds[index].fd = -1;
	poller->connections[index] = NULL;
	return 0;
}


int io_poll_wait(io_poll_poller *poller, unsigned int timeout)
{
	int to;
	
	if(timeout >= INT_MAX) {
		to = -1;
	} else {
		to = timeout;
	}
	
	poller->cnt_fd = poll(poller->pfds, poller->num_pfds, to);
	if(poller->cnt_fd < 0) {
        // it's not an error if we were interrupted.
        if(errno == EINTR) {
            poller->cnt_fd = 0;
        }
	}
	
	return poller->cnt_fd;
}


int io_poll_dispatch(struct io_poller *base_poller)
{
	int i, max, events;
	io_atom *atom;
	io_poll_poller *poller = &base_poller->poller_data.poll;
	
    if(poller->cnt_fd <= 0) {
    	return 0;
    }
    
    max = poller->cnt_fd;
    for(i=0, max=poller->num_pfds; i<max; i++) {
    	events = poller->pfds[i].revents;
    	if(events) {
    		poller->pfds[i].revents = 0;
    		atom = poller->connections[i];
    		if(events | POLLIN) {
    			 (*atom->read_proc)(base_poller, atom);
    		}
    		if(events | POLLOUT)  {
    			 (*atom->write_proc)(base_poller, atom);
    		}
    	}
    }
    
    return 0;
}

#endif

