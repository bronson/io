// multi.h
// Scott Bronson
// 9 Mar 2007

// This file allows you to choose your polling method at run time.
// (it also allows you to use different polling methods in different
// threads, but I don't see much point to that...)

#include "selectop.h"

struct io_poller;

struct poller_funcs {
	int (*exit)(struct io_poller *poller);
	int (*exit_check)(struct io_poller *poller);
	int (*add)(struct io_poller *poller, io_atom *atom, int flags);
	int (*enable)(struct io_poller *poller, io_atom *atom, int flags);
	int (*disable)(struct io_poller *poller, io_atom *atom, int flags);
	int (*set)(struct io_poller *poller, io_atom *atom, int flags);
	int (*del)(struct io_poller *poller, io_atom *atom);
	int (*wait)(struct io_poller *poller, unsigned int timeout);
	void (*dispatch)(struct io_poller *poller);
};


struct io_poller {
	struct poller_funcs funcs;
	union {
		io_select_poller select;
	} u;
};
typedef struct io_poller io_poller;


int io_init(io_poller *poller);
#define io_exit(a) 			(*(a)->funcs.exit)((io_poller*)&(a)->u)
#define io_exit_check(a)	(*(a)->funcs.exit_check)((io_poller*)&(a)->u)
#define io_add(a,b,c)		(*(a)->funcs.add)((io_poller*)&(a)->u,b,c)
#define io_enable(a,b,c)	(*(a)->funcs.enable)((io_poller*)&(a)->u,b,c)
#define io_disable(a,b,c)	(*(a)->funcs.disable)((io_poller*)&(a)->u,b,c)
#define io_set(a,b,c)		(*(a)->funcs.set)((io_poller*)&(a)->u,b,c)
#define io_del(a,b)			(*(a)->funcs.del)((io_poller*)&(a)->u,b)
#define io_wait(a,b)		(*(a)->funcs.wait)((io_poller*)&(a)->u,b)
#define io_dispatch(a)		(*(a)->funcs.dispatch)((io_poller*)&(a)->u)

