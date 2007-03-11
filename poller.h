// multi.h
// Scott Bronson
// 9 Mar 2007

// This is the meta-file that allows you to choose your polling method at run time.
// (it allows you to use different polling methods in different
// threads but I don't see much point to that.)

// TODO: should get rid of this include and just make io_poller's poller_data opaque
#include "pollers/select.h"


struct io_poller;

// Believe you me, this table is *almost* enough to drive me to port this to C++.
// (biggest problem with doing that is having to babysit C++'s memory management)
struct poller_funcs {
	int (*dispose)(struct io_poller *poller);
	int (*fd_check)(struct io_poller *poller);
	int (*add)(struct io_poller *poller, io_atom *atom, int flags);
	int (*remove)(struct io_poller *poller, io_atom *atom);	
	int (*enable)(struct io_poller *poller, io_atom *atom, int flags);
	int (*disable)(struct io_poller *poller, io_atom *atom, int flags);
	int (*set)(struct io_poller *poller, io_atom *atom, int flags);
	int (*wait)(struct io_poller *poller, unsigned int timeout);
	void (*dispatch)(struct io_poller *poller);
};


struct io_poller {
	struct poller_funcs funcs;
	union {
		io_select_poller select;
	} poller_data;
};
typedef struct io_poller io_poller;


int io_poller_init(io_poller *poller);
#define io_poller_dispose(a) (*(a)->funcs.dispose)((io_poller*)&(a)->poller_data)
#define io_fd_check(a)		(*(a)->funcs.fd_check)((io_poller*)&(a)->poller_data)
#define io_add(a,b,c)		(*(a)->funcs.add)((io_poller*)&(a)->poller_data,b,c)
#define io_remove(a,b)		(*(a)->funcs.remove)((io_poller*)&(a)->poller_data,b)
#define io_enable(a,b,c)	(*(a)->funcs.enable)((io_poller*)&(a)->poller_data,b,c)
#define io_disable(a,b,c)	(*(a)->funcs.disable)((io_poller*)&(a)->poller_data,b,c)
#define io_set(a,b,c)		(*(a)->funcs.set)((io_poller*)&(a)->poller_data,b,c)
#define io_wait(a,b)		(*(a)->funcs.wait)((io_poller*)&(a)->poller_data,b)
#define io_dispatch(a)		(*(a)->funcs.dispatch)((io_poller*)&(a)->poller_data)

