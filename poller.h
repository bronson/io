// multi.h
// Scott Bronson
// 9 Mar 2007

// This is the meta-file that allows you to choose your polling method at run time.
// (it allows you to use different polling methods in different
// threads but I don't see much point to that.)

// TODO: should get rid of these include and just make io_poller's
// poller_data opaque!  That would also fix the huge disparity between
// epoll/kqueue's tiny poller_data, and poll/select's huge poller_data.

// TODO: should use a single, static function table for each poller.
// The poller data structure should be a pointer to the static table
// of functions and a void* or int.

// TODO: try to use preprocessor to decide what pollers are even compilable.
#if !(defined(USE_SELECT) || defined(USE_POLL) || defined(USE_EPOLL))
#define USE_SELECT
#endif

#ifdef USE_SELECT
#include "pollers/select.h"
#endif

#ifdef USE_POLL
#include "pollers/poll.h"
#endif

#ifdef USE_EPOLL
#include "pollers/epoll.h"
#endif


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
	const char *poller_name;
	union {
		// TODO: the select struct is WAY bigger than epoll...
		// Should not put them in a union with each other!

#ifdef USE_SELECT
		io_select_poller select;
#endif

#ifdef USE_POLL
		io_poll_poller poll;
#endif

#ifdef USE_EPOLL
		io_epoll_poller epoll;
#endif
		
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
#define io_dispatch(a)		(*(a)->funcs.dispatch)(a)
