// atom.h
// Scott Bronson
// 2 Oct 2003

/** @file atom.h
 *
 * This is the generic Async I/O API.  It can be implemented using
 * select, poll, epoll, kqueue, aio, and /dev/poll (hopefully).
 *
 * TODO: should make it so that we can compile every io method
 * into the code then select the most appropriate one at runtime.
 */

#ifndef IO_H
#define IO_H

/// Flag, tells if we're interested in read events.
#define IO_READ 0x01
/// Flag, tells if we're interested in write events.
#define IO_WRITE 0x02
/// Flag, tells if we're interested in exceptional events.
#define IO_EXCEPT 0x04

// reserved for use by applications.  They can send pseudo-events
// to socket routines.  This could be used, for instance, to send
// higher-level events, such as "data available", "remote has closed
// the connection", etc.  (er, thanks to the new dispatching, this
// capability will probably never be used...)

#define IO_USER1 0x10
#define IO_USER2 0x20
#define IO_USER3 0x40
#define IO_USER4 0x80


/// Tells how many incoming connections we can handle at once
/// (this is just the backlog parameter to listen)
#ifndef STD_LISTEN_SIZE
#define STD_LISTEN_SIZE 128
#endif


struct io_atom;

/**
 * This routine is called whenever there is action on an atom.
 *
 * @param atom The atom that the proc is being called on.
 * @param flags What sort of action is happening.
 */
typedef void (*io_proc)(struct io_atom *atom, int flags);

/**
 * Not all data structures are lucky enough to be able to place the
 * io_atom first.  For those that can't, io_resolve will convert
 * the io_atom pointer back into a pointer to the data structure
 * that contains it.
 *
 * @param ptr The io_atom* that you are trying to convert.
 * @param type The type of data structure that contains the io_atom.
 * @param member The io_atom that is being pointed to.
 *
 * For instance, for this structure:
 *
 *   struct test {
 *      int x;
 *      io_atom a;
 *   };
 *
 * The following line would resolve "io_atom *ap" into a pointer
 * to struct test.
 *
 *   io_resolve(ap, struct test, a);
 *
 */

#define io_parent_type(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


/**
 * This structure is used to keep track of all the filehandles
 * we're watching for events and the functions to call when they
 * occur.  NOTE: you cannot copy io_atoms!!  Once you've called
 * io_add on an atom, it must remain at that memory address until
 * you call io_remove.
 *
 * On the surface, it may seem odd to have so small a structure.
 * Why not just pass the proc and fd to io_add() as arguments
 * and not worry about memory allocation at all?  The reason is
 * that this io_atom will probably be a small part of a much
 * larger structure.  It's easy to convert from the atom back
 * to the larger structure.  Better to keep the atom information
 * with its related data structures than off in an io-specific
 * chunk of memory.
 */
struct io_atom {
	io_proc proc;   ///< The function to call when there is an event on fd.
	int fd;         ///< The fd to watch for events.
};
typedef struct io_atom io_atom;

#define io_atom_init(io,ff,pp) ((io)->fd=(ff),(io)->proc=(pp))

void io_init();     ///< Call this routine once when your code starts.  It prepares the io library for use.
void io_exit();     ///< Call this routine once when your program terminates.  It just releases any resources allocated by io_init.
int io_exit_check();	///< Returns how many fds were leaked.  Also prints them to stderr.


/** Adds the given io_atom to the current watch list.
 *
 * The io_atom must be pre-allocated and exist until you
 * call io_del() on it.
 *
 * @param atom The io_atom to add.
 * @param flags The events to watch for.  Note that there's no way to retrieve the flags once set.
 * @returns The appropriate error code or 0 if there was no error.
 */

int io_add(io_atom *atom, int flags);		///< Adds the given atom to the list of files being watched.
int io_enable(io_atom *atom, int flags);	///< Enables the given flag(s) without affecting any others.
int io_disable(io_atom *atom, int flags);	///< Disables the given flag(s) without affecting any others.
int io_set(io_atom *atom, int flags);   	///< Sets the io_atom::flags on the given atom to flags.
int io_del(io_atom *atom);              	///< Removes the atom from the list 

/// Waits for an event, then handles it.  Stops waiting if a timeout
/// or a signal occurs.
/// Specify MAXINT for no timeout.  The timeout is specified in ms.
/// Returns the number of descriptors waiting for data on success,
/// or a negative number on failure.
int io_wait(unsigned int timeout);
/// Dispatches the events returned by io_wait.  if io_wait returned 0
/// (no events to process), io_dispatch does nothing.
void io_dispatch();


/** Reads data from a socket.
 *
 * This routine handles strange boundary cases like re-trying a read that
 * was interrupted by a signal and returning an error when the remote
 * has closed the connection.  It's just for convenience.
 *
 * @returns The number of bytes that were read.  0 will be returned
 * if there was no data to read and the socket was marked nonblocking
 * (otherwise, if the socket is marked blocking, it will block until
 * data is available for reading).  -EPIPE is returned if the remote
 * connection was closed.  Otherwise, a negative number will be
 * returned for any other sort of error.  Check errno for the reason.
 *
 * @param io The io_atom to read from.
 * @param buf The buffer ot fill with the incoming adata.
 * @param cnt The size of the buffer.  Not more than this number of
 * 		bytes will be written to the buffer.
 */

int io_read(io_atom *io, char *buf, size_t cnt, size_t *len);
int io_write(io_atom *io, char *buf, size_t cnt, size_t *len);

/** Closes an open socket.  The io_atom must have been set up previously using
 * one of io_socket_connect(), io_socket_listen(), or io_socket_accept().
 */

void io_close(io_atom *io);


/** Just a utility function.  This routine tries to parse an integer
 *  and, unlike any of the stdc routines, will actually tell you if
 *  an error occurred or not.
 */

int io_safe_atoi(const char *str, int *num);

#endif

