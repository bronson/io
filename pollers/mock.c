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

#ifdef USE_MOCK

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../poller.h"


typedef struct mock_event_tracker {
	const mock_event *event;
	struct mock_event_tracker *next;
} mock_event_tracker;


// This is just a unique value to stuff into a pointer to indicate an error.
const char mock_error = 'M';


// Converts an event pointer into its index in the current event set.
// If the event is not a part of the current event set, this routine dies.
static int get_event_no(io_mock_poller *poller, const mock_event *event)
{
	long l = (event - poller->event_sets);
	
	if(l < 0 || l >= poller->event_set_size) {
		// can't use die because die calls this routine
		fprintf(stderr, "get_event_no: tried to find number of event %p from %p and got %ld?!\n",
				event, poller->event_sets, l);
		exit(1);
	}
	
	return (int)l;
}


// NOTE: event must be in the currently-executing set.
static const char *describe_event(io_mock_poller *poller, const mock_event *event)
{
	static char buf[512];
	
	snprintf(buf, sizeof(buf), "event %d set %d at %s:%d",
			get_event_no(poller,event), poller->current_event_set,
			event->file, event->line);
	
	return buf;
}


static const char *describe_set(io_mock_poller *poller)
{
	static char buf[512];
	
	snprintf(buf, sizeof(buf), "set %d at %s:%d",
			poller->current_event_set,
			poller->event_sets[0].file, poller->event_sets[0].line);
	
	return buf;
}


static void mock_vmsg(io_mock_poller *poller, FILE *file, const char *fmt, va_list ap)
{
	mock_event_tracker *cur;
	
	fprintf(file, "wait %d: ", poller->current_step);
	vfprintf(file, fmt, ap);
	fprintf(file, "\n");

	for(cur = poller->current_event; cur; cur = cur->next) {
		fprintf(file, "   while handling event %d from set %d at %s:%d\n",
				get_event_no(poller, cur->event),
				poller->current_event_set,
				cur->event->file, cur->event->line);
	}
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
	
	assert(conn);
	
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
static int find_event_by_socket_addr(io_mock_poller *poller, const socket_addr inaddr, mock_event_type restrict_type, const mock_event **event, const char *func)
{
	socket_addr tmpaddr;
	const char *errstr;
	int i, num_previous_events;
	
	num_previous_events = 0;
	for(i=0; i<poller->event_set_size; i++) {
		if(poller->event_sets[i].event_type == restrict_type) {
			if(!poller->event_sets[i].data) {
				die(poller, "%s: event requires the address, not NULL: %s",
						func, describe_event(poller, &poller->event_sets[i]));
			}
			if(poller->event_sets[i].data == &mock_error) {
				die(poller, "%s: %s requires an address, not MOCK_ERROR.  "
						"(you can specify an integer after the address to return an error).",
						func, describe_event(poller, &poller->event_sets[i]));
			}
			errstr = io_parse_address(poller->event_sets[i].data, &tmpaddr);
			if(errstr) {
				die(poller, errstr, poller->event_sets[i].data);
			}
			
			if((tmpaddr.addr.s_addr == inaddr.addr.s_addr) && (tmpaddr.port == inaddr.port)) {
				if(poller->events_handled_in_last_set & (1<<i)) {
					// found a matching event but it's already been used...
					// keep searching.
					num_previous_events += 1;
				} else {
					// found it!
					*event = &poller->event_sets[i];
					return 0;
				}
			}
		}
	}

	die(poller, "%s: could not find unused event corresponding to %s:%d "
			"(%d matching events were previously handled)", func,
			inet_ntoa(inaddr.addr), inaddr.port, num_previous_events);
	
	return -1;
}


// Given an mfd, finds the first unused event associated with it.
// (finds the first unused event with the same connection as the mfd)
static int find_event_by_mockfd(io_mock_poller *poller, mockfd *mfd, mock_event_type restrict_type, const mock_event **event, const char *func)
{
	int i, num_previous_events;

	num_previous_events = 0;
	for(i=0; i<poller->event_set_size; i++) {
		if(poller->event_sets[i].event_type == restrict_type) {		
			if(poller->event_sets[i].remote == mfd->remote) {
				if(poller->events_handled_in_last_set & (1<<i)) {
					// found a matching event but it's already been used... keep searching.
					num_previous_events += 1;
				} else {
					// found it!
					*event = &poller->event_sets[i];
					return 0;		
				}
			}
		}
	}
	
	die(poller, "%s: could not find unused event corresponding to connection %s ",
			func, mfd->remote->name);
	
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
	
	info(poller, "Initialized mock poller %p with space for %d fds.",
			poller, MAX_MOCK_FDS);
	
	return 0;
}


int io_mock_poller_dispose(io_mock_poller *poller)
{
	// TODO: check for and warn about any FDs still open.
	// (that will require fixing io_fd_check first)
	// see mock_test_is_finished below for a caller in a similar situation.
	
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
	char str[1024];
	int i;
	
	if(poller->events_handled_in_last_set == (1<<poller->num_events_in_last_set)-1) {
		// application handled each event in the last set
		return;
	}

	// not all events were handled...  generate a readable error message.
	str[0] = '\0';
	for(i=0; i<poller->num_events_in_last_set; i++) {
		if(!((1<<i) & poller->events_handled_in_last_set)) {
			if(str[0] != '\0') {
				strncat(str, ", ", sizeof(str)-1);
			}
			strncat(str, describe_event(poller, &poller->event_sets[i]), sizeof(str)-1);
		}
	}

	die(poller, "io_wait: application didn't handle all %d events in %s, missed: %s",
			poller->num_events_in_last_set, describe_set(poller), str);
}


static int prepare_next_step(io_mock_poller *poller)
{
	int i, dispatched_count, total_count;
	mock_event_type type;

	// count the number of events in the upcoming event set.
	dispatched_count = total_count = 0;
	for(i=0; i<poller->event_set_size; i++) {
		type = poller->event_sets[i].event_type;
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

static void mock_test_is_finished(io_mock_poller *poller, const mock_event *event)
{
	// TODO: check for and warn about any FDs still open.
	// (that will require fixing io_fd_check first)
	
	// It's sub-optimal if we hit this.  It would be a lot better
	// if the user inserts events into the queue that cause his
	// application to quit on its own.  But, since that's not always
	// realistic, we'll allow bailing out here.
	
	info(poller, "io_mock_wait: hit mock_finished event at %s",
			describe_event(poller, event));
	exit(0);
}

int io_mock_wait(io_mock_poller *poller, unsigned int timeout)
{	
	if(!poller->event_sets) {
		fprintf(stderr, "io_wait has no events!");
		exit(1);
	}
	
	// We want to ensure that all events have been used up (i.e. client
	// app didn't forget to read or write some data).  Since
	// we can't mark the events themselves, we'll just keep a count.
	check_that_all_events_were_handled(poller);

	poller->current_step += 1;
	poller->current_event_set += 1;
	poller->event_sets_remaining -= 1;
	poller->event_sets += poller->event_set_size;
	
	if(poller->event_sets[0].event_type == mock_finished) {
		// This event set is done.
		// TODO: offer the client the ability to insert more events.
		mock_test_is_finished(poller, &poller->event_sets[0]);
	}
	
	info(poller, "io_wait: now on io_wait %d", poller->current_step);
	return prepare_next_step(poller);
}


static void mark_event_used(io_mock_poller *poller, const mock_event *event)
{
	int event_no = get_event_no(poller, event);
	
	poller->events_handled_in_last_set |= 1<<event_no;
}


// Ensures that an event hasn't been handled yet, then marks the event as handled.
static void using_event(struct io_mock_poller *poller,
		const mock_event *event, mock_event_tracker *storage, const char *func)
{
	int event_no = get_event_no(poller, event);
	
	if((1<<event_no) & poller->events_handled_in_last_set) {
		die(poller, "%s: event %d has already been handled!", func, event_no);
	}

	mark_event_used(poller, event);
	
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


static int dispatch_atom(struct io_poller *base_poller, const mock_event *event, mockfd *mfd, int flag)
{
	mock_event_tracker storage;
	io_mock_poller *poller = &base_poller->poller_data.mock;
	const char *op = (flag & IO_READ ? "read" : "write");
	
	using_event(poller, event, &storage, "io_dispatch");
	
	if(!(mfd->flags & flag)) {
		die(poller, "event %d contains a %s event but atom's %s events aren't enabled!",
				get_event_no(poller, event), op, op);
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
	if(flag & IO_READ) {
		(*mfd->io->read_proc)(base_poller, mfd->io);
	} else {
		(*mfd->io->write_proc)(base_poller, mfd->io);
	}
	
	done_with_event(poller, &storage);

	return 0;
}


int io_mock_dispatch(struct io_poller *base_poller)
{
	io_mock_poller *poller = &base_poller->poller_data.mock;
	const mock_event *event;
	mockfd *mfd;
	int i, err;
	
	for(i=0; i<poller->event_set_size; i++) {
		event = &poller->event_sets[i];
		if(is_dispatched_mock_event(event->event_type)) {
			if(!event->remote) {
				die(poller, "io_dispatch: dispatched %s has NULL connection.  You must specify a connection to receive the event.", describe_event(poller, event));
			}
			mfd = find_mockfd_by_connection(poller, event->remote);
			if(!mfd) {
				die(poller, "io_dispatch: unknown connection %s, has it been opened or added yet?", event->remote->name);
			}

			switch(event->event_type) {
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
	}
	
	return 0;
}


static int post_error_event(io_mock_poller *poller, const mock_event *event, const char *func)
{
	// We'll just mark this event used right now.  That way the caller
	// doesn't have to worry about anything more; it can return the error
	// immediately.
	mark_event_used(poller, event);
	
	info(poller, "%s: returning error %d (%s) as specified by %s", func,
			event->len, strerror(event->len), describe_event(poller, event));
	
	return event->len;
}


// Returns true if this event specifies that an error should be returned.
static int is_error_event(io_mock_poller *poller, const mock_event *event, const char *func)
{
	// Current convention is that if data is NULL, then the len gives
	// the error code.  That might change so always use is_error_event.
	if(event->data == &mock_error) {
		return post_error_event(poller, event, func);
	}
	
	return 0;
}


// connect and listen need to treat errors slightly differently since you still need to
// specify an address (to associate them with a particular connection).
static int is_addressed_error_event(io_mock_poller *poller, const mock_event *event, const char *func)
{
	// Len is 0 unless you want to mock an error.  It's otherwise unused
	// since the calls just use 0-terminated strings to specify the address.
	if(event->len != 0) {
		return post_error_event(poller, event, func);
	}
	
	return 0;
}


static int find_io_event(io_mock_poller *poller, io_atom *io, mock_event_type type, mockfd **mfdp, const mock_event **evp, const char *func)
{
	int err;
	
	err = find_mockfd_by_atom(poller, io, mfdp, func);
	if(err != 0) return err;
	
	// You'll never call io_read or io_write on a listening socket.
	assert(!(**mfdp).is_listener);
	
	err = find_event_by_mockfd(poller, *mfdp, type, evp, func);
	if(err != 0) return err;

	err = is_error_event(poller, *evp, func);
	if(err) return err;
	
	return 0;
}


int io_mock_read(struct io_poller *base_poller, struct io_atom *io, char *buf, size_t cnt, size_t *readlen)
{
	static const char *func = "io_read";
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	mockfd *mfd;
	const mock_event *event;
	int err;
	
	*readlen = 0;
	
	err = find_io_event(poller, io, mock_read, &mfd, &event, func);
	if(err != 0) return err;
	
	// special-case 0-length reads, which mean that the remote has closed.
	if(event->data && event->len == 0) {
		info(poller, "%s: returning EPIPE (0-length read means EOF) as specified by %s", func,
				describe_event(poller, event));
		mark_event_used(poller, event);
		// IO Atom library's convention: normal close returns EPIPE.
		return EPIPE;
	}
	
	using_event(poller, event, &storage, func);
	 	
	if(cnt < event->len) {
		die(poller, "%s: event data (%d) was larger than read buffer (%d)!",
				func, event->len, (int)cnt);
	}
	if(cnt > event->len) {
		cnt = event->len;
	}
	
	info(poller, "%s: read %d bytes from fd %d", func, cnt, mfd->io->fd);

	memcpy(buf, event->data, cnt);
	
	*readlen = cnt;
	done_with_event(poller, &storage);
	return 0;
}


static void copy_readv_data(io_mock_poller *poller, const mock_event *event, const struct iovec *vec, int cnt, int fd, const char *func)
{
	const char *cp;
	int len, remaining;
	int i;

	cp = event->data;
	remaining = event->len;
	
	if(cnt < 1) {
		die(poller, "%s: cnt was %d, an illegal value!", func, cnt);
	}
	
	for(i=0; i<cnt && remaining > 0; i++) {
		len = vec[i].iov_len;
		if(len > remaining) {
			len = remaining;
		}
		
		memcpy(vec[i].iov_base, cp, len);
		cp += vec[i].iov_len;
		remaining -= len;
	}
	
	info(poller, "%s: read %d bytes from fd %d into %d vectors", func, event->len, fd, i+1);
}


int io_mock_readv(struct io_poller *base_poller, struct io_atom *io, const struct iovec *vec, int cnt, size_t *readlen)
{
	static const char *func = "io_readv";
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	mockfd *mfd;
	const mock_event *event;
	int err;
	
	*readlen = 0;
		
	err = find_io_event(poller, io, mock_readv, &mfd, &event, func);
	if(err != 0) return err;

	// special-case 0-length reads, which mean that the remote has closed.  (this is also true for readv, right?)
	if(event->data && event->len == 0) {
		info(poller, "%s: returning EPIPE (0-length read means EOF) as specified by %s", func,
				describe_event(poller, event));
		mark_event_used(poller, event);
		// IO Atom library's convention: normal close returns EPIPE.
		return EPIPE;
	}
	
	using_event(poller, event, &storage, func);

	copy_readv_data(poller, event, vec, cnt, mfd->io->fd, func);
	
	*readlen = event->len;
	done_with_event(poller, &storage);
	return 0;
}


int io_mock_write(struct io_poller *base_poller, struct io_atom *io, const char *buf, size_t cnt, size_t *wrlen)
{
	static const char *func = "io_write";
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	mockfd *mfd;
	const mock_event *event;
	int err;

	*wrlen = 0;
	
	err = find_io_event(poller, io, mock_write, &mfd, &event, func);
	if(err != 0) return err;
	
	using_event(poller, event, &storage, func);

	if(cnt > event->len) {
		die(poller, "%s: event data %d was larger than write buffer %d!",
				func, event->len, (int)cnt);
	}
	
	if(memcmp(buf, event->data, event->len)) {
		die(poller, "%s: event data was different.  evt=%s buf=%s",
				func, event->data, buf);
	}
	
	info(poller, "%s: wrote %d bytes to fd %d",
			func, event->len, mfd->io->fd);
	
	*wrlen = event->len;
	done_with_event(poller, &storage);
	return 0;	
}


static void verify_writev_data(io_mock_poller *poller, const mock_event *event, const struct iovec *vec, int cnt, int fd, const char *func)
{
	const char *cp;
	int len, remaining, total;
	int i;

	total = 0;
	cp = event->data;
	remaining = event->len;
	
	if(cnt < 1) {
		die(poller, "%s: cnt was %d, an illegal value!", func, cnt);
	}
	
	for(i=0; i<cnt && remaining > 0; i++) {
		len = vec[i].iov_len;
		if(len > remaining) {
			len = remaining;
		}
		
		if(memcmp(vec[i].iov_base, cp, len)) {
			die(poller, "%s: vector[%d]'s event data was different.  evt=%s buf=%s",
					func, i, cp, vec[i].iov_base);
		}
		
		cp += vec[i].iov_len;
		remaining -= len;
		total += len;
	}
	
	if(remaining != 0 || i != cnt) {
		die(poller, "%s: vector only provided %d bytes of data but we expected %d!",
				func, total, event->len);
	}
	
	info(poller, "%s: verified %d bytes written to fd %d from %d vectors", func, event->len, fd, i+1);
}


int io_mock_writev(struct io_poller *base_poller, struct io_atom *io, const struct iovec *vec, int cnt, size_t *wrlen)
{
	static const char *func = "io_write";
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	mockfd *mfd;
	const mock_event *event;
	int err;

	*wrlen = 0;
	
	err = find_io_event(poller, io, mock_writev, &mfd, &event, func);
	if(err != 0) return err;
	
	using_event(poller, event, &storage, func);
	
	verify_writev_data(poller, event, vec, cnt, mfd->io->fd, func);

	*wrlen = event->len;
	done_with_event(poller, &storage);
	return 0;	
}


void parse_socket_address(io_mock_poller *poller, socket_addr *saddr, const char *str)
{
	const char *errstr;

	// find address where connection is originating from.
	saddr->addr.s_addr = htonl(INADDR_ANY);
	saddr->port = -1;
	
	errstr = io_parse_address(str, saddr);
	if(errstr) {
		die(poller, errstr, str);
	}
	
	if(saddr->port == -1) {
		die(poller, "port number is required but one wasn't found in %s!", str);
	}
}


int io_mock_connect(struct io_poller *base_poller, io_atom *io, io_proc read_proc, io_proc write_proc, socket_addr remote, int flags)
{
	static const char *func = "io_connect";
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	const mock_event *event;
	socket_addr tmpaddr;
	int fd, err;
	
	io_atom_init(io, -1, NULL, NULL);
	err = find_event_by_socket_addr(poller, remote, mock_connect, &event, func);
	if(err < 0) {
		return err;
	}
	
	assert(event->remote->type == mock_socket);
	
	err = is_addressed_error_event(poller, event, func);
	if(err) return err;

	using_event(poller, event, &storage, func);

	// quick sanity check to ensure that the originating and destination
	// addresses are not the same (since the code can't currently retrieve
	// the originating address of the socket, this is only academic for now).
	parse_socket_address(poller, &tmpaddr, event->remote->source_address);
	if((tmpaddr.addr.s_addr == remote.addr.s_addr) && (tmpaddr.port == remote.port)) {
		die(poller, "%s: event's originating address %s:%d needs to be different from its destination address %s:%d for %s", func,
				inet_ntoa(tmpaddr.addr), tmpaddr.port,
				inet_ntoa(remote.addr), remote.port,
				describe_event(poller,event));
	}
	
	fd = mock_open(poller);
	io_atom_init(io, fd, read_proc, write_proc);
	mock_fd_install(poller, fd, io, event->remote, flags, 0);
	
	info(poller, "%s: opened fd %d to %s:%d",
			func, fd, inet_ntoa(remote.addr), remote.port);

	done_with_event(poller, &storage);
	
	return 0;
}


int io_mock_accept(struct io_poller *base_poller, io_atom *io, io_proc read_proc, io_proc write_proc, int flags, io_atom *listener, socket_addr *remote)
{
	static const char *func = "io_accept";
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	const mock_event *event;
	int fd, err;
	mockfd *mfd;
	socket_addr toaddr, fromaddr;
	char buf[512];
	
	io_atom_init(io, -1, NULL, NULL);
	err = find_mockfd_by_atom(poller, listener, &mfd, func);
	if(err != 0) return err;
	
	if(!mfd->is_listener) {
		die(poller, "Called %s on %s without first calling io_listen!",
				func, mfd->remote->name);
	}
	
	parse_socket_address(poller, &toaddr, mfd->remote->source_address);
	err = find_event_by_socket_addr(poller, toaddr, mock_accept, &event, func);
	if(err != 0) return err;
	
	if(!mfd->is_listener) {
		die(poller, "%s but connection %s is not listening!?", func, mfd->remote->name);
	}
	
	assert(event->remote->type == mock_socket);
	assert(mfd->remote->type == mock_socket);
	
	err = is_addressed_error_event(poller, event, func);
	if(err) return err;
	
	using_event(poller, event, &storage, func);

	fd = mock_open(poller);
	io_atom_init(io, fd, read_proc, write_proc);
	mock_fd_install(poller, fd, io, event->remote, flags, 0);

	parse_socket_address(poller, &fromaddr, event->remote->source_address);		
	if(remote) {
		*remote = fromaddr;
	}
	
	// sanity check: make sure the source and destination addresses aren't the same.
	if((fromaddr.addr.s_addr == toaddr.addr.s_addr) && (fromaddr.port == toaddr.port)) {
		die(poller, "%s: event source address %s:%d needs to be different from its destination address %s:%d for %s", func,
				inet_ntoa(fromaddr.addr), fromaddr.port,
				inet_ntoa(toaddr.addr), toaddr.port,
				describe_event(poller,event));
	}
	
	// hack to get around inet_ntoa's use of a static buffer...
	strncpy(buf, inet_ntoa(toaddr.addr), sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	info(poller, "%s: opened fd %d for inbound connection to %s:%d from %s:%d",
			func, fd, buf, toaddr.port, inet_ntoa(fromaddr.addr), fromaddr.port);
	
	done_with_event(poller, &storage);
	return 0;
}


int io_mock_listen(struct io_poller *base_poller, io_atom *io, io_proc read_proc, socket_addr local, int reuse_addr)
{
	static const char *func = "io_listen";
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mock_event_tracker storage;
	const mock_event *event;
	socket_addr tmpaddr;
	int fd, err;
	
	io_atom_init(io, -1, NULL, NULL);
	err = find_event_by_socket_addr(poller, local, mock_listen, &event, func);
	if(err < 0) {
		return err;
	}
	assert(event->remote->type == mock_socket);	

	err = is_addressed_error_event(poller, event, func);
	if(err) return err;

	using_event(poller, event, &storage, func);
	
	// quick sanity check to ensure that the originating and destination
	// addresses are the same (this is only true for listening sockets).
	parse_socket_address(poller, &tmpaddr, event->remote->source_address);
	if((tmpaddr.addr.s_addr != local.addr.s_addr) || (tmpaddr.port != local.port)) {
		die(poller, "%s: event listen address %s:%d needs to match its connection address %s:%d for %s", func,
				inet_ntoa(tmpaddr.addr), tmpaddr.port,
				inet_ntoa(local.addr), local.port,
				describe_event(poller,event));
	}

	fd = mock_open(poller);
	assert(!poller->mockfds[fd].is_listener);  // if this is true then we'd already called io_listen on this atom?!
	io_atom_init(io, fd, read_proc, NULL);
	mock_fd_install(poller, fd, io, event->remote, IO_READ, 1);
	assert(poller->mockfds[fd].remote->type == mock_socket);
		
	info(poller, "%s: opened fd %d listening on %s",
			func, fd, event->remote->source_address);

	done_with_event(poller, &storage);
	return 0;
}


int io_mock_close(struct io_poller *base_poller, io_atom *io)
{
	static const char *func = "io_close";
	io_mock_poller *poller = &base_poller->poller_data.mock;
	mockfd *mfd;
	const mock_event *event;
	mock_event_tracker storage;
	int err;

	err = find_mockfd_by_atom(poller, io, &mfd, func);
	if(err != 0) {
		return err;
	}

	err = find_event_by_mockfd(poller, mfd, mock_close, &event, func);
	if(err != 0) return err;

	err = is_error_event(poller, event, func);
	if(err) return err;

	using_event(poller, event, &storage, func);

	info(poller, "%s: closing fd %d", func, io->fd);

	mock_fd_reset(mfd);
	io->fd = -1;
	
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
	
	info(poller, "Adding %d sets of events.  Max events per set is %d.",
			n_events, events->max_events_per_set);
	
	poller->event_set_size = events->max_events_per_set;
	poller->event_sets = &events->events[0][0];
	poller->event_sets_remaining = n_events;
	poller->current_event_set = 0;
	
	prepare_next_step(poller);
	
	return 0;
}

#endif
