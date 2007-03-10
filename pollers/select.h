// selectop.h
// Scott Bronson
// 9 Mar 2007

// Data structures and prototypes used by the select poller.

// NOTE: don't use anything in this file if you want your code 
// to be portable to different pollers.  Just write to the generic
// polling interface and include select.h to use select directly,
// or use multi.h to dynamically choose a polling method.

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "../atom.h"


struct io_select_poller {
	io_atom* connections[FD_SETSIZE];
	fd_set fd_read, fd_write, fd_except;
	
	// Need to dispatch from a copy of the fd tables so that the
	// io_procs can delete atoms w/o causing errors (picture deleting
	// an atom that has an event on it but hasn't been processed yet).
	// TODO: A much more elegant solution would be for io_delete to queue up delete
	// requests if we're in the middle of dispatching and then handle
	// them all in one go right before io_dispatch returns.
	fd_set gfd_read, gfd_write, gfd_except;
	
	int max_fd;	// the highest-numbered filedescriptor in connections.
	int cnt_fd; // the number of fds that have events on them
};
typedef struct io_select_poller io_select_poller;


int io_select_init(io_select_poller *poller);
int io_select_exit(io_select_poller *poller);
int io_select_exit_check(io_select_poller *poller);
int io_select_add(io_select_poller *poller, io_atom *atom, int flags);
int io_select_set(io_select_poller *poller, io_atom *atom, int flags);
int io_select_enable(io_select_poller *poller, io_atom *atom, int flags);
int io_select_disable(io_select_poller *poller, io_atom *atom, int flags);
int io_select_del(io_select_poller *poller, io_atom *atom);
int io_select_wait(io_select_poller *poller, unsigned int timeout);
int io_select_dispatch(io_select_poller *poller);

