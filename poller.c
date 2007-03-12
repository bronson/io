// poller.c
// Scott Bronson
// 8 Mar 2007

// Allows you to select what poller you'd like to use at runtime.

#include <assert.h>
#include "poller.h"

int io_poller_init(io_poller *poller)
{
	// TODO: make this routine select a proper poller at runtime.
	// TODO: this routine should probably also take an argument
	//		to allow the caller to select the poller to use.

	//			epoll
/*	if(IO_EPOLL_INCLUDED) {
		poller->poller_name = "epoll";
		poller->funcs.dispose = (void*)io_epoll_poller_dispose;
		poller->funcs.fd_check = (void*)io_epoll_fd_check;
		poller->funcs.add = (void*)io_epoll_add;
		poller->funcs.remove = (void*)io_epoll_remove;
		poller->funcs.enable = (void*)io_epoll_enable;
		poller->funcs.disable = (void*)io_epoll_disable;
		poller->funcs.set = (void*)io_epoll_set;
		poller->funcs.wait = (void*)io_epoll_wait;
		poller->funcs.dispatch = (void*)io_epoll_dispatch;
		return io_epoll_init(&poller->poller_data.epoll);
	} */
	
	//			poll
#ifdef USE_POLL
	if(1) {
		poller->poller_name = "poll";
		poller->funcs.dispose = (void*)io_poll_poller_dispose;
		poller->funcs.fd_check = (void*)io_poll_fd_check;
		poller->funcs.add = (void*)io_poll_add;
		poller->funcs.remove = (void*)io_poll_remove;
		poller->funcs.enable = (void*)io_poll_enable;
		poller->funcs.disable = (void*)io_poll_disable;
		poller->funcs.set = (void*)io_poll_set;
		poller->funcs.wait = (void*)io_poll_wait;
		poller->funcs.dispatch = (void*)io_poll_dispatch;
		return io_poll_init(&poller->poller_data.poll);
	}
#endif

	//			select
#ifdef USE_SELECT
	if(1) {
		poller->poller_name = "select";
		poller->funcs.dispose = (void*)io_select_poller_dispose;
		poller->funcs.fd_check = (void*)io_select_fd_check;
		poller->funcs.add = (void*)io_select_add;
		poller->funcs.remove = (void*)io_select_remove;
		poller->funcs.enable = (void*)io_select_enable;
		poller->funcs.disable = (void*)io_select_disable;
		poller->funcs.set = (void*)io_select_set;
		poller->funcs.wait = (void*)io_select_wait;
		poller->funcs.dispatch = (void*)io_select_dispatch;
		return io_select_init(&poller->poller_data.select);
	}
#endif
	
	assert(!"You did not select a poller!");
	return -1;
}
