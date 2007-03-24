// mock.c
// Scott Bronson
// 22 May 2007
//
// Mocks a poller so that you can manually generate read and
// write events.  This can be used to exercise your event-based
// code from within unit tests.

#include "../poller.h"


int io_mock_init(io_mock_poller *poller)
{
	int i;
	
	for(i=0; i<MAX_MOCK_FDS; i++) {
		poller->atoms[i].fd = -1;		
		poller->atoms[i].flags = 0;
		poller->atoms[i].atom = NULL;
	}
	
	return 0;
}


int io_mock_poller_dispose(io_mock_poller *poller)
{
	// nothing to do
	return 0;
}


int io_mock_fd_check(io_mock_poller *poller)
{
	// TODO: when fd_check is rewritten
	return 0;
}


int io_mock_add(io_mock_poller *poller, io_atom *atom, int flags)
{
	assert(!"don't call mock_add!");
	return 0;
}


int io_mock_set(io_mock_poller *poller, io_atom *atom, int flags)
{
	int fd = atom->fd;

	if(fd < 0 || fd > MAX_MOCK_FDS) {
		return -ERANGE;
	}
	if(poller->atoms[fd]) {
		return -EALREADY;
	}
}


int io_mock_remove(io_mock_poller *poller, io_atom *atom)
{
	assert(!"don't call mock_remove!");
	return 0;
}


int io_mock_wait(io_mock_poller *poller, unsigned int timeout)
{
	return 0;
}


int io_mock_dispatch(struct io_poller *base_poller)
{
	return 0;
}


int io_mock_open(io_mock_poller *poller)
{
	for(i=0; i<MAX_MOCK_FDS; i++) {
		if(poller->atoms[i].fd == -1) {	
			return i;
		}
	}
	
	errno = EMFILE;
	return -1;
}


void io_mock_init(io_poller *poller, io_atom *atom, io_proc read, io_proc write)
{
	io_atom_init(atom, io_mock_open(poller), read, write);
}

