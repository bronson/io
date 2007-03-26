// mock.h
// Scott Bronson
// 22 Mar 2007

#include "../atom.h"
#include "../socket.h"


// mock fds are numbered from 0 .. MAX_MOCK_ATOMS-1
#define MAX_MOCK_FDS 32

struct mock_event;
struct mock_event_tracker;

typedef enum {
	mock_connection_unknown,
	mock_socket,
	mock_file,		// TODO
	mock_directory	// TODO
} mock_connection_type;

typedef struct mock_connection {
	const char *name;
	mock_connection_type type;
	const char *source_address;
} mock_connection;

typedef struct mock_atom {
	io_atom *io;			///< the atom associated with this entry.
	const mock_connection *remote; ///< tells what mock connection this atom belongs to.
	int flags;				///< gives the current io_set flags for this atom.
	int is_listener;		///< true if this is a socket listening for connections, false if it's something else.
/*	int last_read_step;		///< gives the value of io_mock_poller::current_step when the last read event on this atom was posted.
	int last_write_step; STEPSTEP */
} mockfd;

typedef struct io_mock_poller {
	struct mock_event_tracker *current_event;
	const struct mock_event *event_sets;	///< the events that will be dispatched the next time io_wait is called.
	int current_step;				///< monotonically increasing counter that increments each time wait is called.
	int event_sets_remaining;		///< tells how many events remain in event_sets.
	int events_handled_in_last_set;	///< a bitfield that tracks which events have been run.  if (1<<n) is set, then event n has been run.
	int num_events_in_last_set;		///< the number of events in the previous event set.  Ensures all events from previous set have been run before we start on the next set.
	int event_set_size;
	mockfd mockfds[MAX_MOCK_FDS];
} io_mock_poller;


int io_mock_init(io_mock_poller *poller);
int io_mock_poller_dispose(io_mock_poller *poller);
int io_mock_fd_check(io_mock_poller *poller);
int io_mock_add(io_mock_poller *poller, io_atom *atom, int flags);
int io_mock_set(io_mock_poller *poller, io_atom *atom, int flags);
int io_mock_remove(io_mock_poller *poller, io_atom *atom);
int io_mock_wait(struct io_poller *poller, unsigned int timeout);
int io_mock_dispatch(struct io_poller *poller);

int io_mock_read(struct io_poller *poller, struct io_atom *io, char *buf, size_t cnt, size_t *readlen);
int io_mock_write(struct io_poller *poller, struct io_atom *io, const char *buf, size_t cnt, size_t *wrlen);
int io_mock_connect(struct io_poller *poller, io_atom *io, io_proc read_proc, io_proc write_proc, socket_addr remote, int flags);
int io_mock_accept(struct io_poller *poller, io_atom *io, io_proc read_proc, io_proc write_proc, int flags, io_atom *listener, socket_addr *remote);
int io_mock_listen(struct io_poller *poller, io_atom *io, io_proc read_proc, socket_addr local);


/*
/// Returns the fd of an appropriate unused mock connection.
/// If you've used up all connections, returns -1 and sets errno to EMFILE.
int io_mock_open(io_poller *poller);
void io_mock_init(io_poller *poller, io_atom *atom, io_proc read, io_proc write);

void io_mock_queue_read(io_poller *poller, int fd, char *data, int size);
void io_mock_queue_string(io_poller *poller, int fd, char *data);
void io_mock_queue_write(io_poller *poller, int fd);
*/


// The following are all to define mock events



#ifndef MAX_EVENTS_PER_SET
/** Unfortunately there's no way in C to define a ragged-right
 * two-dimensional array.  So, we'll say that, by default, you
 * can't dispatch more than 4 events per wait loop.  If you want
 * more, you can define MAX_EVENTS_PER_SET to whatever you want
 * before including the mock headers.
 * This macro is just unfortunate...  thanks C99.
 */
#define MAX_EVENTS_PER_SET 8
#endif


/** Allows you to specify data as a constant C string, rather
 * than having to specify string and length separately.
 */

#define MOCK_DATA(str)	(str),sizeof(str)-1
#define MOCK_ERROR(err)	NULL,(err)

// THIS MACRO ABSOLUTELY SUCKS!!  And so do the mock_event_types.
// Should write macro opcodes to handle all this cruft...  TODO.
#define EVENT			__FILE__,__LINE__

typedef enum {
	mock_nop = 0,
	
	// These match up with calls made
	mock_listen,		///> application will call listen
	mock_connect,		///> application will attempt to make a connection
	mock_accept,		///> send a connection request to an application that called io_listen
	mock_read,     		///> tells mock that the application will read some data without an event being posted (not sure why)
	mock_write,			///> tells mock that the application will write some data even though no write event was posted (this is very common; most data is written in response to read events)

	// these cause events to be generated
	mock_event_read,	///> tells mock to post a read event, but doesn't specify any data (this is probably always a mistake... tell me if you use this!)
	mock_event_write,	///> tells mock to post a write event, but the application won't actually write any data.

	mock_finished		///> the null terminator that marks the end of the mock event sets.
} mock_event_type;

#define is_dispatched_mock_event(event_type) (event_type >= mock_event_read)


/** Describes a single event that can be dispatched by a poller.
 * This includes a read event, a write event, a connection event
 * (which looks just like a read event), etc.
 */

typedef struct mock_event {
	const char *file;
	int line;
	mock_event_type event_type;
	const mock_connection *remote;	// right now this is just the address of the remote system.  in the future this will need to include the pathname of a file or folder that's been opened.
	const char *data;	// on connect or accept, the address to connect to as a string
	int len;			// on connect or accept, the port to connect to
} mock_event;


/** An event queue is a list of events for multiple invocations
 * of the io_wait/io_dispatch calls.
 * 
 * One element in the queue is an event set.  This represets a
 * collection of events that should be considered simultaneous.
 * All events in a set will be dispatched together.
 */

typedef struct mock_event_queue {
	int max_events_per_set;		///< always set this to MAX_EVENTS_PER_SET.
	mock_event events[][MAX_EVENTS_PER_SET];
} mock_event_queue;



int io_mock_set_events(struct io_poller *poller, const mock_event_queue *events);

