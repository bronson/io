/* select.h
 * Scott Bronson
 * 9 Mar 2007
 * 
 * Include this file to compile a program that polls using only select().
 */

#include "selectop.h"


struct io_poller {
	union {
		io_select_poller select;
	} u;
};
typedef struct io_poller io_poller;


#if 1

#define io_init(a) 			io_select_init(&(a)->u.select)
#define io_exit(a) 			io_select_exit(&(a)->u.select)
#define io_exit_check(a)	io_select_exit_check(&(a)->u.select)
#define io_add(a,b,c)		io_select_add(&(a)->u.select,b,c)
#define io_enable(a,b,c)	io_select_enable(&(a)->u.select,b,c)
#define io_disable(a,b,c)	io_select_disable(&(a)->u.select,b,c)
#define io_set(a,b,c)		io_select_set(&(a)->u.select,b,c)
#define io_del(a,b)			io_select_del(&(a)->u.select,b)
#define io_wait(a,b)		io_select_wait(&(a)->u.select,b)
#define io_dispatch(a)		io_select_dispatch(&(a)->u.select)

#else

static inline int io_init(io_poller *poller, void* (allocator)(size_t)) {
	return io_select_init(&poller->u.select, allocator);
}

static inline int io_exit(io_poller *poller) {
	return io_select_init(&poller->u.select);
}

#endif

