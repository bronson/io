// select.c
// Scott Bronson
// 4 October 2003
//
// Uses select to satisfy gatekeeper's network I/O
// Because of select's internal limitations, MAXFDS is 1024.
//
// This code is licensed under the same terms as Parrot itself.


#include <stdio.h>
#include <errno.h>
#include <values.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "io.h"


static io_atom* connections[FD_SETSIZE];
static fd_set fd_read, fd_write, fd_except;
static int max_fd;	// the highest-numbered filedescriptor in connections.


// Pass the file descriptor that you'll be listening and accepting on.

void io_init()
{
	FD_ZERO(&fd_read);
	FD_ZERO(&fd_write);
	FD_ZERO(&fd_except);
}


void io_exit()
{
	// nothing to do
}


static void install(int fd, int flags)
{
	if(flags & IO_READ) {
		FD_SET(fd, &fd_read);
	} else {
		FD_CLR(fd, &fd_read);
	}

	if(flags & IO_WRITE) {
		FD_SET(fd, &fd_write);
	} else {
		FD_CLR(fd, &fd_write);
	}

	if(flags & IO_EXCEPT) {
		FD_SET(fd, &fd_except);
	} else {
		FD_CLR(fd, &fd_except);
	}
}


int io_add(io_atom *atom, int flags)
{
	int fd = atom->fd;

	if(fd < 0 || fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(connections[fd]) {
		return -EALREADY;
	}

	connections[fd] = atom;
	install(fd, flags);
	if(fd > max_fd) max_fd = fd;
	
	return 0;
}


int io_set(io_atom *atom, int flags)
{
	int fd = atom->fd;

	if(fd < 0 || fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(!connections[fd]) {
		return -EALREADY;
	}

	install(fd, flags);

	return 0;
}


int io_del(io_atom *atom)
{
	int fd = atom->fd;

	if(fd < 0 || fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(!connections[fd]) {
		return -EALREADY;
	}

	install(fd, 0);
	connections[fd] = NULL;

	while((max_fd >= 0) && (connections[max_fd] == NULL))  {
		max_fd -= 1;
	}

	return 0;
}


// Wait for events, then dispatch them.
// timeout is in milliseconds.  MAXINT == forever.

int io_wait(int timeout)
{
	struct timeval tv;
	struct timeval *tvp = &tv;
	int num, i, max, flags;

	if(timeout == MAXINT) {
		tvp = NULL;
	} else {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	fd_set rfds = fd_read;
	fd_set wfds = fd_write;
	fd_set efds = fd_except;

	num = select(1+max_fd, &rfds, &wfds, &efds, tvp);
	if(num < 0) {
		perror("select");
		return num;
	}

    // Note that max_fd might change in the middle of this loop.
    // For instance, if an acceptor proc opens a new connection
    // and calls io_add, max_fd will take on the new value.  Therefore,
    // we need to save the old value in a local temporary.
	for(i=0, max=max_fd; i <= max; i++) {
		flags = 0;
		if(FD_ISSET(i, &rfds)) flags |= IO_READ;
		if(FD_ISSET(i, &wfds)) flags |= IO_WRITE;
		if(FD_ISSET(i, &efds)) flags |= IO_EXCEPT;
		if(flags) {
			if(connections[i]) {
				(*connections[i]->proc)(connections[i], flags);
			} else {
				// what do we do -- event on an unknown connection?
				printf("Got an event on an uknown connection %d!\n", i);
			}
		}
	}

	return num;
}

