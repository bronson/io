// mock.c
// Scott Bronson
// 22 May 2007
//
// Mocks a poller so that you can manually generate read and
// write events.  This can be used to exercise your event-based
// code from within unit tests.


// TODO: add code that will print a warning if events occur out of order.
// i.e. my event loop should be 100% deterministic so I should be
// able to order the events in the script to exactly match the events
// as they occur in the program.


#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../poller.h"


typedef struct mock_event_tracker {
	mock_event *event;
	struct mock_event_tracker *next;
} mock_event_tracker;


// Converts an event pointer into its index in the current event set.
// If the event is not a part of the current event set, this routine dies.
static int get_event_no(io_mock_poller *poller, mock_event *event)
{
	long l = (event - *poller->event_sets);
	
	// this is not a necessary assumption but it appears to always be true...
	assert(event == poller->current_event->event);
	
	if(l < 0 || l >= poller->event_set_size) {
		// can't use die because die calls this routine
		fprintf(stderr, "Could not not get number of event %p, base is %p\n",
				event, *poller->event_sets);
		exit(1);
	}
	
	return (int)l;
}


static void mock_vmsg(io_mock_poller *poller, FILE *file, const char *fmt, va_list ap)
{
	mock_event_tracker *cur;
	
	fprintf(file, "step %d:", poller->current_step);
	for(cur = poller->current_event; cur; cur = cur->next) {
		fprintf(file, "   as part of %s:%d event %d",
				cur->event->file, cur->event->line,
				get_event_no(poller, cur->event));
	}
	
	vfprintf(file, fmt, ap);
	fprintf(file, "\n");
}


// Print an informative message
static void info(io_mock_poller *poller, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	mock_vmsg(poller, stdout, fmt, ap);
	va_end(ap);
}


// Say some last words and stop
static void die(io_mock_poller *poller, const char *fmt, ...)
{
	va_list ap;

    va_start(ap, fmt);
	mock_vmsg(poller, stderr, fmt, ap);
    va_end(ap);

	exit(1);
}


// Finds the first free mockfd or dies if there are no free fds. 
static int mock_open(io_mock_poller *poller)
{
	int i;
	
	for(i=0; i<MAX_MOCK_FDS; i++) {
		if(!poller->mockfds[i].io) {	
			return i;
		}
	}
	
	die(poller, "mock_open: could not find an fd to use!  Out of fds?");
	return -1;
}


// Converts a flags value into a human-readable string.
static const char* show_flags(int flags)
{
	static const char *strs[] = {
		"rw", "Rw", "rW", "RW"
	};
	
	if(flags >= 0 && flags < sizeof(strs)/sizeof(strs[0])) {
		return strs[flags];
	}
	
	return "??";
}


// Resets a mockfd back to its pristine state
static void mock_fd_reset(mockfd *mfd)
{
	mfd->io = NULL;
	mfd->remote = NULL;
	mfd->flags = 0;
	mfd->is_listener = 0;
/*	mfd->last_read_step = 0;
	mfd->last_write_step = 0; STEPSTEP */
}


static void mock_fd_install(io_mock_poller *poller, int fd,
		io_atom *io, const mock_connection *conn, int flags, int is_listener)
{
	mockfd *mfd = &poller->mockfds[fd];

	mfd->io = io;
	if(conn) {
		mfd->remote = conn;
	} else {
		if(!mfd->remote) {
			die(poller, "mock_fd_install: connection needs to be already set if you're not setting it.");
		}
	}
	mfd->flags = flags;
	if(is_listener != -1) {
		mfd->is_listener = is_listener;
	}
}


// used to find the mockfd associated with a connection in an event.
static mockfd* find_mockfd_by_connection(io_mock_poller *poller, const mock_connection *conn)
{
	int i;
	
	// a linear search through the fds each time...  arg.
	// this sucks, but anything else is overkill.
	for(i=0; i<MAX_MOCK_FDS; i++) {
		if(poller->mockfds[i].remote == conn) {
			return &poller->mockfds[i];
		}
	}
	
	return NULL;
}


// used to find the mockfd associated with an atom in an IO Atom call.
// this should just be an array dereference... this call does some
// additional sanity checking.
static int find_mockfd_by_atom(io_mock_poller *poller, const io_atom *io, mockfd **mfd, const char *func)
{
	if(io->fd < 0 || io->fd >= MAX_MOCK_FDS) {
		info(poller, "%s on atom %p fd=%d failed: fd out of range!", func, io, io->fd);
		return -ERANGE;
	}
	if(!poller->mockfds[io->fd].io) {
		info(poller, "%s on atom %p fd=%d failed: not added!", func, io, io->fd);
		return -EEXIST;
	}
	
	*mfd = &poller->mockfds[io->fd];
	assert((*mfd)->io == io);
	
	return 0;
}


// Given a socket address, this routine finds the first unused mock_listen or mock_connect event associated with that address.
static int find_event_by_socket_addr(io_mock_poller *poller, const socket_addr inaddr, mock_event **event, const char *func)
{
	socket_addr tmpaddr;
	const char *errstr;
	int i, num_previous_events;
	
	num_previous_events = 0;
	for(i=0; i<poller->event_set_size; i++) {
		switch(poller->event_sets[0][i].event_type) {
		case mock_listen:
		case mock_connect:
			errstr = io_parse_address(poller->event_sets[0][i].data, &tmpaddr);
			if(errstr) {
				die(poller, errstr, poller->event_sets[0][i].data);
			}
			
			if((tmpaddr.addr.s_addr == inaddr.addr.s_addr) && (tmpaddr.port == inaddr.port)) {
				if(poller->events_handled_in_last_set & (1<<i)) {
					// found a matching event but it's already been used...
					// keep searching.
					num_previous_events += 1;
				} else {
					// found it!
					*event = &poller->event_sets[0][i];
					return 0;
				}
			}
			break;
		
		default:
			// no need to do anything because this event doesn't contain
			// a socket addresss.
			break;
		}
	}

	die(poller, "%s: could not find unused event corresponding to %s:%d "
			"(%d matching events were previously handled)", func,
			inet_ntoa(inaddr.addr), inaddr.port, num_previous_events);
	
	return -1;
}


// Given an mfd, finds the first unused event associated with it.
// (finds the first unused event with the same connection as the mfd)
static int find_event_by_mockfd(io_mock_poller *poller, mockfd *mfd, mock_event **event, const char *func)
{
	int i, num_previous_events;

	num_previous_events = 0;
	for(i=0; i<poller->event_set_size; i++) {
		switch(poller->event_sets[0][i].event_type) {		
		case mock_listen: case mock_connect: case mock_accept:
		case mock_read: case mock_write:
			if(poller->event_sets[0][i].remote == mfd->remote) {
				if(poller->events_handled_in_last_set & (1<<i)) {
					// found a matching event but it's already been used... keep searching.
					num_previous_events += 1;
				} else {
					// found it!
					*event = &poller->event_sets[0][i];
					return 0;		
				}
			}
			break;

		default:
			// none of these other events use the mfd.
			break;
		}
	}
	
	die(poller, "%s: could not find unused event corresponding to connection %s "
			"(%d matching events were previously handled)", func,
			mfd->remote->name, num_previous_events);
	
	return -1;

}


int io_mock_init(io_mock_poller *poller)
{
	int i;
	
	poller->current_event = NULL;
	poller->event_sets = NULL;
	poller->current_step = 0;
	poller->event_sets_remaining = 0;
	poller->events_handled_in_last_set = 0;
	poller->num_events_in_last_set = 0;
	
	for(i=0; i<MAX_MOCK_FDS; i++) {
		mock_fd_reset(&poller->mockfds[i]);
	}
	
	info(poller, "Initialized mock poller %p with space %d fds.",
			poller, MAX_MOCK_FDS);
	
	return 0;
}


int io_mock_poller_dispose(io_mock_poller *poller)
{
	// TODO: ensure all connections are closed.
	
	info(poller, "Disposed of mock poller %p.", poller);
	return 0;
}


int io_mock_fd_check(io_mock_poller *poller)
{
	info(poller, "io_mock_fd_check is not implemented!");
	return 0;
}


int io_mock_add(io_mock_poller *poller, io_atom *io, int flags)
{
	mockfd *mfd;
	int err;
	
	err = find_mockfd_by_atom(poller, io, &mfd, "io_add");
	if(err != 0) {
		return err;
	}
	
	mock_fd_install(poller, io->fd, NULL, NULL, flags, -1);
	info(poller, "io_add: fd=%d atom=%p flags=%d (%s)",
			io->fd, io, flags, show_flags(flags));
	
	return 0;
}


int io_mock_set(io_mock_poller *poller, io_atom *io, int flags)
{
	mockfd *mfd;
	int err;
	
	err = find_mockfd_by_atom(poller, io, &mfd, "io_set");
	if(err != 0) {
		return err;
	}
	
	info(poller, "io_set: setting flags on fd %d from %d (%s) to %d (%s)", io->fd,
			mfd->flags, show_flags(mfd->flags), flags, show_flags(flags));
	mfd->flags = flags;

	return 0;
}


int io_mock_remove(io_mock_poller *poller, io_atom *io)
{
	mockfd *mfd;
	int err;

	err = find_mockfd_by_atom(poller, io, &mfd, "io_remove");
	if(err != 0) {
		return err;
	}

	info(poller, "io_remove: removing fd %d", io->fd);
	mock_fd_reset(mfd);
	
	return 0;
}


static void check_that_all_events_were_handled(io_mock_poller *poller)
{
	char num[32];
	char str[1024];
	int i;
	
	if(poller->events_handled_in_last_set == (1<<poller->num_events_in_last_set)-1) {
		// application handled each event in the last set
		return;
	}

	// not all events were handled...  generate a readable error message.
	str[0] = '\0';
	for(i=0; i<poller->event_set_size; i++) {
		if(!((1<<i) & poller->events_handled_in_last_set)) {
			snprintf(num, sizeof(num), "%d", i);
			if(str[0] != '\0') {
				strncat(str, ",", sizeof(str)-1);
			}
			strncat(str, num, sizeof(str)-1);
		}
	}

	die(poller, "io_wait: application didn't handle all %d events in last set!  missed: %s",
			poller->num_events_in_last_set, str);
}


int io_mock_wait(io_poller *base_poller, unsigned int timeout)
{
	io_mock_poller *poller = &base_poller->poller_data.mock;
	int i, dispatched_count, total_count;
	mock_event_type type;
	
	if(!poller->event_sets) {
		fprintf(stderr, "io_wait has no events!");
		exit(1);
	}
	
	// We want to ensure that all events have been used up (i.e. client
	// app didn't forget to read or write some data).  Since
	// we can't mark the events themselves, we'll just keep a count.
	check_that_all_events_were_handled(poller);

	poller->current_step += 1;
	info(poller, "io_wait: now on step %d", poller->current_step);
	
	// count the number of events in the upcoming event set.
	dispatched_count = total_count = 0;
	for(i=0; i<poller->event_set_size; i++) {
		type = poller->event_sets[0][i].event_type;
		if(is_dispatched_mock_event(type)) {
			dispatched_count += 1;
		}
		if(type != mock_nop) {
			total_count += 1;
		}
	}
	
	poller->events_handled_in_last_set = 0;
	poller->num_events_in_last_set = total_count;
	
	return dispatched_count;
}


// Ensures that an event hasn't been handled yet, then marks the event as handled.
static void using_event(struct io_mock_poller *poller,
		mock_event *event, mock_event_tracker *storage, const char *func)
{
	int event_no = get_event_no(poller, event);
	
	if((1<<event_no) & poller->events_handled_in_last_set) {
		die(poller, "%s: event %d has already been handled!", func, event_no);
	}

	poller->events_handled_in_last_set |= 1<<event_no;
	
	// link this event on top of the queue of in-progress events.
	storage->event = event;
	storage->next = poller->current_event;
	poller->current_event = storage;
}


static void done_with_event(struct io_mock_poller *poller, mock_event_tracker *storage)
{
	assert(storage == poller->current_event);	// if not, then someone probably screwed up the nesting
	assert(storage);							// if not, there were probably multiple frees.
	
	poller->current_event = storage->next;
}


static int dispatch_atom(struct io_poller *base_poller, mock_event *event, mockfd *mfd, int flag)
{
	mock_event_tracker storage;
	io_mock_poller *poller = &base_poller->poller_data.mock;
	const char *op = (flag & IO_READ ? "read" : "write");
	
	using_event(poller, event, &storage, "io_dispatch");
	
	if(!(mfd->flags & flag)) {
		die(poller, "mock write event but atom's write events aren't enabled!");
	}
	
	/* STEPSTEP
	if(mfd->last_write_step >= poller->current_step) {
		die(poller, "mock %s event has already been handled!  "
				"current step is %d, atom's last %s was %d",
				op, poller->current_step, op, mfd->last_write_step);
	}
	mfd->last_step = poller->current_step;
	*/
	
	info(poller, "io_dispatch: dispatching %s event on %d", op, mfd->io->fd);
	(*mfd->io->write_proc)(base_poller, mfd->io);
	
	done_with_event(poller, &storage);

	return 0;
}


int io_mock_dispatch(struct io_poller *base_poller)
{
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event *event;
	mockfd *mfd;
	int i, type, err;
	
	for(i=0; i<poller->event_set_size; i++) {
		event = &poller->event_sets[0][i];
		mfd = find_mockfd_by_connection(poller, event->remote);
		if(!mfd) {
			die(poller, "io_mock_dispatch: unknown connection %s, has it been opened or added yet?",
					event->remote->name);
		}
		
		switch(type) {
		case mock_event_read:
			err = dispatch_atom(base_poller, event, mfd, IO_READ);
			break;
		case mock_event_write:
			err = dispatch_atom(base_poller, event, mfd, IO_WRITE);
			break;
		default:
			// This "event" doesn't involve anything actually
			// being dispatched to the io_atom's callbacks.
			break;
		}
		
		if(err) {
			return err;
		}
	}
	
	poller->event_set_size -= 1;
	poller->event_sets += 1;
	
	return 0;
}


int io_mock_read(struct io_poller *base_poller, struct io_atom *io, char *buf, size_t cnt, size_t *readlen)
{
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	mockfd *mfd;
	mock_event *event;
	int err;
	
	*readlen = 0;
	
	err = find_mockfd_by_atom(poller, io, &mfd, "io_read");
	if(err != 0) return err;
	assert(!mfd->is_listener);
	
	err = find_event_by_mockfd(poller, mfd, &event, "io_read");
	if(err != 0) return err;

	using_event(poller, event, &storage, "io_read");
	
	if(cnt < event->len) {
		die(poller, "io_read: event data (%d) was larger than read buffer (%d)!",
				event->len, (int)cnt);
	}
	
	memcpy(buf, event->data, cnt);
	*readlen = cnt;
	done_with_event(poller, &storage);
	return 0;
}


int io_mock_write(struct io_poller *base_poller, struct io_atom *io, const char *buf, size_t cnt, size_t *wrlen)
{
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	mockfd *mfd;
	mock_event *event;
	int err;

	*wrlen = 0;
	
	err = find_mockfd_by_atom(poller, io, &mfd, "io_write");
	if(err != 0) return err;
	assert(!mfd->is_listener);
	
	err = find_event_by_mockfd(poller, mfd, &event, "io_write");
	if(err != 0) return err;

	using_event(poller, event, &storage, "io_write");

	if(cnt > event->len) {
		die(poller, "io_write: event data %d was larger than write buffer %d!",
				event->len, (int)cnt);
	}
	
	if(memcmp(buf, event->data, event->len)) {
		die(poller, "io_write: event data was different.  evt=%s buf=%s",
				event->data, buf);
	}
	
	*wrlen = event->len;
	done_with_event(poller, &storage);
	return 0;	
}


int io_mock_connect(struct io_poller *base_poller, io_atom *io, io_proc read_proc, io_proc write_proc, socket_addr remote, int flags)
{
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	mock_event *event;
	int fd, err;
	
	err = find_event_by_socket_addr(poller, remote, &event, "io_connect");
	if(err < 0) {
		return err;
	}
	
	assert(event->remote->type == mock_socket);
	assert(poller->mockfds[fd].remote->type == mock_socket);
	assert(!poller->mockfds[fd].is_listener);
	
	using_event(poller, event, &storage, "io_connect");
	
	fd = mock_open(poller);
	io_atom_init(io, fd, read_proc, write_proc);
	mock_fd_install(poller, fd, io, event->remote, flags, 0);
	
	done_with_event(poller, &storage);
	
	return 0;
}


int io_mock_accept(struct io_poller *base_poller, io_atom *io, io_proc read_proc, io_proc write_proc, int flags, io_atom *listener, socket_addr *remote)
{
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	mock_event *event;
	int fd, err;
	mockfd *mfd;
	const char *errstr;
	
	err = find_mockfd_by_atom(poller, io, &mfd, "io_accept");
	if(err != 0) return err;
	
	if(!mfd->is_listener) {
		die(poller, "Called io_accept on %s without first calling io_listen!",
				mfd->remote->name);
	}
	
	err = find_event_by_mockfd(poller, mfd, &event, "io_accept");
	if(err != 0) return err;
	
	assert(event->remote->type == mock_socket);
	assert(mfd->remote->type == mock_socket);
	assert(mfd->is_listener);
	
	using_event(poller, event, &storage, "io_accept");
	
	fd = mock_open(poller);
	io_atom_init(io, fd, read_proc, write_proc);
	mock_fd_install(poller, fd, io, event->remote, flags, 0);
	
	if(remote) {
		remote->addr.s_addr = htonl(INADDR_ANY);
		remote->port = -1;
		errstr = io_parse_address(event->remote->source_address, remote);
		if(errstr) {
			die(poller, errstr, event->remote->source_address);
		}
		if(remote->port == -1) {
			die(poller, "Connection %s didn't provide a remote port number: %s!",
					event->remote->name, event->remote->source_address);
		}
	}
	
	done_with_event(poller, &storage);
	return 0;
}


int io_mock_listen(struct io_poller *base_poller, io_atom *io, io_proc read_proc, socket_addr local)
{
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	mock_event *event;
	int fd, err;
	
	err = find_event_by_socket_addr(poller, local, &event, "io_listen");
	if(err < 0) {
		return err;
	}

	assert(event->remote->type == mock_socket);
	assert(poller->mockfds[fd].remote->type == mock_socket);
	assert(!poller->mockfds[fd].is_listener);  // if this is true then we'd already called io_listen on this atom?!
	
	using_event(poller, event, &storage, "io_listen");

	fd = mock_open(poller);
	io_atom_init(io, fd, read_proc, NULL);
	mock_fd_install(poller, fd, io, event->remote, IO_READ, 1);
	
	done_with_event(poller, &storage);
	return 0;
}


int io_mock_set_events(io_poller *base_poller, const mock_event_queue *events)
{
	io_mock_poller *poller = &base_poller->poller_data.mock;
	int event_capacity;
	int n_events;
	
	// quick sanity check to make sure we can fit all events from the
	// largest set into the bitfield that tracks which events have been run.
	event_capacity = 8 * sizeof(int);	// assuming io_mock_poller.num_events_in_last_set is also an int.
	if(event_capacity < events->max_events_per_set) {
		die(poller, "io_mock_poller.events_handled_in_last_set (%d) is too small to store MAX_EVENTS_PER_SET (%d). "
				"You probably want to shrink MAX_EVENTS_PER_SET.",
				event_capacity, events->max_events_per_set);
	}
	
	// Count the number of steps in this event queue.
	// The null terminator must come at the start of its own event set.
	for(n_events = 0; ; n_events++ ) {
		if(events->events[n_events][0].event_type == mock_finished) {
			break;
		}
	}
	
	info(poller, "Adding events %p comprising %d steps.  Max events per step is %d.",
			events, n_events, events->max_events_per_set);
	
	poller->event_set_size = events->max_events_per_set;
	poller->event_sets = (mock_event**)events->events;
	poller->event_sets_remaining = n_events;
	
	return 0;
}
