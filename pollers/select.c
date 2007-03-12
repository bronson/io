// select.c
// Scott Bronson
// 4 October 2003
//
// Uses select to poll for I/O events.
// Because of select's internal limitations, MAXFDS is 1024.


// TODO: should probably make thread-safe by passing a global
// context everywhere?  This is pretty far in the future...

#ifdef USE_SELECT

#include <stdlib.h>
#include <errno.h>
#include <values.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../poller.h"


// Pass the file descriptor that you'll be listening and accepting on.

int io_select_init(io_select_poller *poller)
{
	poller->max_fd = 0;
	
	FD_ZERO(&poller->fd_read);
	FD_ZERO(&poller->fd_write);
	FD_ZERO(&poller->fd_except);
	
	return 0;
}


int io_select_poller_dispose(io_select_poller *poller)
{
	// nothing to dispose of
	return 0;
}


int io_select_fd_check(io_select_poller *poller)
{
	int cnt = 0;
	int i;

	// Check that we haven't leaked any atoms.
	for(i=0; i<FD_SETSIZE; i++) {
		if(poller->connections[i]) {
// TODO: need to figure out some other way of returning this info to the caller.
//			fprintf(stderr, "Leaked atom fd=%d proc=%08lX!\n", i, (long)poller->connections[i]);
			cnt += 1;
		}
	}

	return cnt;
}


static void install(io_select_poller *poller, int fd, int flags)
{
	if(flags & IO_READ) {
		FD_SET(fd, &poller->fd_read);
	} else {
		FD_CLR(fd, &poller->fd_read);
	}

	if(flags & IO_WRITE) {
		FD_SET(fd, &poller->fd_write);
	} else {
		FD_CLR(fd, &poller->fd_write);
	}

	if(flags & IO_EXCEPT) {
		FD_SET(fd, &poller->fd_except);
	} else {
		FD_CLR(fd, &poller->fd_except);
	}
}


int io_select_add(io_select_poller *poller, io_atom *atom, int flags)
{
	int fd = atom->fd;

	if(fd < 0 || fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(poller->connections[fd]) {
		return -EALREADY;
	}

	poller->connections[fd] = atom;
	install(poller, fd, flags);
	if(fd > poller->max_fd) {
		poller->max_fd = fd;
	}
	
	return 0;
}


int io_select_set(io_select_poller *poller, io_atom *atom, int flags)
{
	int fd = atom->fd;

	if(fd < 0 || fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(!poller->connections[fd]) {
		return -EEXIST;
	}

	install(poller, fd, flags);

	return 0;
}


int io_select_remove(io_select_poller *poller, io_atom *atom)
{
	int fd = atom->fd;

	if(fd < 0 || fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(!poller->connections[fd]) {
		return -EALREADY;
	}

	install(poller, fd, 0);
	poller->connections[fd] = NULL;

    // This io_delete is probably during an io_process.  Therefore,
    // we need to make sure that we don't later report an event
    // on a deleted io_atom.
    FD_CLR(fd, &poller->gfd_read);
    FD_CLR(fd, &poller->gfd_write);
    FD_CLR(fd, &poller->gfd_except);

	while((poller->max_fd >= 0) && (poller->connections[poller->max_fd] == NULL))  {
		poller->max_fd -= 1;
	}

	return 0;
}


/** Waits for events.  See io_dispatch to dispatch the events.
 * 
 * @param timeout The maximum amount of time we should wait in
 * milliseconds.  MAXINT is special-cased to mean forever.
 *
 * @returns the number of events to be dispatched or a negative
 * number if there was an error.  If a signal interrupted the
 * call then we return 0 (no error, but no fds need attention).
 * Also, we return 0 if the call returned due to a timeout.
 */

int io_select_wait(io_select_poller *poller, unsigned int timeout)
{
	struct timeval tv;
	struct timeval *tvp = &tv;

	if(timeout == MAXINT) {
		tvp = NULL;
	} else {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	poller->gfd_read = poller->fd_read;
	poller->gfd_write = poller->fd_write;
	poller->gfd_except = poller->fd_except;

	poller->cnt_fd = select(1+poller->max_fd, &poller->gfd_read, &poller->gfd_write, &poller->gfd_except, tvp);
    if(poller->cnt_fd < 0) {
        // it's not an error if we were interrupted.
        if(errno == EINTR) {
            poller->cnt_fd = 0;
        }
    }

    return poller->cnt_fd;
}


int io_select_dispatch(struct io_poller *base_poller)
{
	int i, max, flags;
	io_select_poller *poller = &base_poller->poller_data.select;

    // Note that max_fd might change in the middle of this loop.
    // For instance, if an acceptor proc opens a new connection
    // and calls io_add, max_fd will take on the new value.  Therefore,
    // we need to loop on the value set at the start of the loop.

    if(poller->cnt_fd <= 0) {
    	return 0;
    }
    
    max = poller->max_fd;
    for(i=0; i <= max; i++) {
        flags = 0;
        if(FD_ISSET(i, &poller->gfd_read)) flags |= IO_READ;
        if(FD_ISSET(i, &poller->gfd_write)) flags |= IO_WRITE;
        if(FD_ISSET(i, &poller->gfd_except)) flags |= IO_EXCEPT;
        if(flags) {
            if(poller->connections[i]) {
                (*poller->connections[i]->proc)(base_poller, poller->connections[i], flags);
            } else {
                // what do we do -- event on a null connection??
            	// That's got to be an internal error.  TODO TODO
            }
        }
    }
    
    return 0;
}

#endif

