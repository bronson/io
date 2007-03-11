#include "poller.h"


// This routine allows you to preallocate a poller that can listen
// to the given number of connections.
//size_t io_sizeof(int nconns)
//{
//}


int io_poller_init(io_poller *poller)
{
	poller->funcs.dispose = (void*)io_select_poller_dispose;
	poller->funcs.fd_check = (void*)io_select_fd_check;
	poller->funcs.add = (void*)io_select_add;
	poller->funcs.remove = (void*)io_select_remove;
	poller->funcs.enable = (void*)io_select_enable;
	poller->funcs.disable = (void*)io_select_disable;
	poller->funcs.set = (void*)io_select_set;
	poller->funcs.wait = (void*)io_select_wait;
	poller->funcs.dispatch = (void*)io_select_dispatch;
	
	return 0;
}
