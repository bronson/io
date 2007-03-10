#include "multi.h"


// This routine allows you to preallocate a poller that can listen
// to the given number of connections.
//size_t io_sizeof(int nconns)
//{
//}


int io_init(io_poller *poller)
{
	poller->funcs.exit = (void*)io_select_exit;
	poller->funcs.exit_check = (void*)io_select_exit_check;
	poller->funcs.add = (void*)io_select_add;
	poller->funcs.enable = (void*)io_select_enable;
	poller->funcs.disable = (void*)io_select_disable;
	poller->funcs.set = (void*)io_select_set;
	poller->funcs.del = (void*)io_select_del;
	poller->funcs.wait = (void*)io_select_wait;
	poller->funcs.dispatch = (void*)io_select_dispatch;
	
	return 0;
}
