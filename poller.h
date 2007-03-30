// multi.h
// Scott Bronson
// 9 Mar 2007

// This is the meta-file that allows you to choose your polling method at run time.
// (it allows you to use different polling methods in different
// threads but I don't see much point to that.)

// TODO: should get rid of these include and just make io_poller's
// poller_data opaque!  That would also fix the huge disparity between
// epoll/kqueue's tiny poller_data, and poll/select's huge poller_data.
// Each poller would just get a few void*s or something, and would have
// malloc its own data.

// TODO: should use a single, static function table for each poller.
// The poller data structure should be a pointer to the static table
// of functions and a void* or int.  (true?  isn't this method faster,
// especially on multiprocessor systems that have to synch cache lines?)

// TODO: try to use preprocessor to decide what pollers are even compilable.


#include "atom.h"
#include "socket.h"

#ifndef POLLER_H
#define POLLER_H

#if !(defined(USE_SELECT) || defined(USE_POLL) || defined(USE_EPOLL))
// lowest common denominator, available on all platforms
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

#ifdef USE_MOCK
#include "pollers/mock.h"
#endif


struct io_poller;

typedef enum {
	IO_POLLER_NONE = 0x00,
	IO_POLLER_SELECT= 0x01,
	IO_POLLER_POLL = 0x02,
	IO_POLLER_EPOLL = 0x04,
	IO_POLLER_KQUEUE = 0x08,
	IO_POLLER_MOCK = 0x80,

	IO_POLLER_ANY = 0xFF,		///< chooses the fastest poller for the platform
	IO_POLLER_LINUX = IO_POLLER_SELECT | IO_POLLER_POLL | IO_POLLER_EPOLL,
	IO_POLLER_BSD = IO_POLLER_SELECT | IO_POLLER_POLL | IO_POLLER_KQUEUE
} io_poller_type;


// Believe you me, this table is *almost* enough to drive me to port this to C++.
// (biggest problem with doing that is having to babysit C++'s memory management)
struct io_poller_funcs {
	int (*dispose)(struct io_poller *poller);
	int (*fd_check)(struct io_poller *poller);
	int (*add)(struct io_poller *poller, io_atom *atom, int flags);
	int (*remove)(struct io_poller *poller, io_atom *atom);	
	int (*set)(struct io_poller *poller, io_atom *atom, int flags);
	int (*wait)(struct io_poller *poller, unsigned int timeout);
	int (*dispatch)(struct io_poller *poller);
	int (*read)(struct io_poller *poller, struct io_atom *io, char *buf, size_t cnt, size_t *readlen);
	int (*write)(struct io_poller *poller, struct io_atom *io, const char *buf, size_t cnt, size_t *wrlen);
	int (*connect)(struct io_poller *poller, io_atom *io, io_proc read_proc, io_proc write_proc, socket_addr remote, int flags);
	int (*accept)(struct io_poller *poller, io_atom *io, io_proc read_proc, io_proc write_proc, int flags, io_atom *listener, socket_addr *remote);
	int (*listen)(struct io_poller *poller, io_atom *io, io_proc read_proc, socket_addr local);
	int (*close)(struct io_poller *poller, io_atom *io);
};


struct io_poller {
	struct io_poller_funcs funcs;
	const char *poller_name;
	io_poller_type poller_type;
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
		
#ifdef USE_MOCK
		io_mock_poller mock;
#endif
		
	} poller_data;
};
typedef struct io_poller io_poller;


int io_poller_init(io_poller *poller, io_poller_type type);
#define io_poller_dispose(a) (*(a)->funcs.dispose)((io_poller*)&(a)->poller_data)
#define io_fd_check(a)		(*(a)->funcs.fd_check)((io_poller*)&(a)->poller_data)
#define io_add(a,b,c)		(*(a)->funcs.add)((io_poller*)&(a)->poller_data,b,c)
#define io_remove(a,b)		(*(a)->funcs.remove)((io_poller*)&(a)->poller_data,b)
#define io_set(a,b,c)		(*(a)->funcs.set)((io_poller*)&(a)->poller_data,b,c)
#define io_wait(a,b)		(*(a)->funcs.wait)((io_poller*)&(a)->poller_data,b)
#define io_dispatch(a)		(*(a)->funcs.dispatch)(a)
#define io_read(a,io,buf,cnt,rdlen)   (*(a)->funcs.read)(a,io,buf,cnt,rdlen)
#define io_write(a,io,buf,cnt,wrlen)  (*(a)->funcs.write)(a,io,buf,cnt,wrlen)
#define io_connect(a,io,rp,wp,ra,f)   (*(a)->funcs.connect)(a,io,rp,wp,ra,f)
#define io_accept(a,io,rp,wp,f,l,r)   (*(a)->funcs.accept)(a,io,rp,wp,f,l,r)
#define io_listen(a,io,rp,l)          (*(a)->funcs.listen)(a,io,rp,l)
#define io_close(a,io)      (*(a)->funcs.close)(a,io)

#define io_is_mock(a)	((a)->poller_type == IO_POLLER_MOCK)
#endif
