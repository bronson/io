// poller.c
// Scott Bronson
// 8 Mar 2007

// Allows you to select what poller you'd like to use at runtime.

#include <string.h>
#include "poller.h"


/**
 * @param type: specifies what types of poller you want to
 * create.  For instance, POLLER_SELECT|POLLER_EPOLL, or
 * POLLER_LINUX.
 */


int io_poller_init(io_poller *poller, io_poller_type type)
{
	// TODO: make this routine select a proper poller at runtime.
	// TODO: this routine should probably also take an argument
	//		to allow the caller to select the poller to use.

	memset(poller, 0, sizeof(io_poller));
	
	// These are handled by identical routines in all pollers
	// except for the mock poller.
	poller->funcs.read = io_atom_read;
	poller->funcs.write = io_atom_write;
	poller->funcs.connect = io_socket_connect;
	poller->funcs.accept = io_socket_accept;
	poller->funcs.listen = io_socket_listen;
	poller->funcs.close = io_atom_close;

#ifdef USE_EPOLL
	if(type & IO_POLLER_EPOLL) {
		poller->poller_name = "epoll";
		poller->poller_type = IO_POLLER_EPOLL;
		poller->funcs.dispose = (void*)io_epoll_poller_dispose;
		poller->funcs.fd_check = (void*)io_epoll_fd_check;
		poller->funcs.add = (void*)io_epoll_add;
		poller->funcs.remove = (void*)io_epoll_remove;
		poller->funcs.set = (void*)io_epoll_set;
		poller->funcs.wait = (void*)io_epoll_wait;
		poller->funcs.dispatch = (void*)io_epoll_dispatch;
		return io_epoll_init(&poller->poller_data.epoll);
	}
#endif

	
#ifdef USE_POLL
	if(type & IO_POLLER_POLL) {
		poller->poller_name = "poll";
		poller->poller_type = IO_POLLER_POLL;
		poller->funcs.dispose = (void*)io_poll_poller_dispose;
		poller->funcs.fd_check = (void*)io_poll_fd_check;
		poller->funcs.add = (void*)io_poll_add;
		poller->funcs.remove = (void*)io_poll_remove;
		poller->funcs.set = (void*)io_poll_set;
		poller->funcs.wait = (void*)io_poll_wait;
		poller->funcs.dispatch = (void*)io_poll_dispatch;
		return io_poll_init(&poller->poller_data.poll);
	}
#endif

	
#ifdef USE_SELECT
	if(type & IO_POLLER_SELECT) {
		poller->poller_name = "select";
		poller->poller_type = IO_POLLER_SELECT;
		poller->funcs.dispose = (void*)io_select_poller_dispose;
		poller->funcs.fd_check = (void*)io_select_fd_check;
		poller->funcs.add = (void*)io_select_add;
		poller->funcs.remove = (void*)io_select_remove;
		poller->funcs.set = (void*)io_select_set;
		poller->funcs.wait = (void*)io_select_wait;
		poller->funcs.dispatch = (void*)io_select_dispatch;
		return io_select_init(&poller->poller_data.select);
	}
#endif
	
	
#ifdef USE_MOCK
	if(type & IO_POLLER_MOCK) {
		poller->poller_name = "mock";
		poller->poller_type = IO_POLLER_MOCK;
		poller->funcs.dispose = (void*)io_mock_poller_dispose;
		poller->funcs.fd_check = (void*)io_mock_fd_check;
		poller->funcs.add = (void*)io_mock_add;
		poller->funcs.remove = (void*)io_mock_remove;
		poller->funcs.set = (void*)io_mock_set;
		poller->funcs.wait = (void*)io_mock_wait;
		poller->funcs.dispatch = (void*)io_mock_dispatch;
		
		poller->funcs.read = io_mock_read;
		poller->funcs.write = io_mock_write;
		poller->funcs.connect = io_mock_connect;
		poller->funcs.accept = io_mock_accept;
		poller->funcs.listen = io_mock_listen;
		poller->funcs.close = io_mock_close;
		return io_mock_init(&poller->poller_data.mock);
	}
#endif
	

	return -1;
}
