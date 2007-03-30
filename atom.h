// atom.h
// Scott Bronson
// 2 Oct 2003

/** @file atom.h
 *
 * This is the generic Async I/O API.  It can be implemented using
 * select, poll, epoll, kqueue, aio, and /dev/poll (hopefully).
 *
 * TODO: we currently push all OS requests down to the client.
 * That's bad form.  We should figure out what the most common
 * errors will be and try to make a generic way to handle them.
 * TODO: should make it so that we can compile every io method
 * into the code then select the most appropriate one at runtime.
 */

#ifndef IO_ATOM_H
#define IO_ATOM_H

#include <sys/types.h>
#include <errno.h>


/// Flag, tells if we're interested in read events.
#define IO_READ 0x01
/// Flag, tells if we're interested in write events.
#define IO_WRITE 0x02


struct io_atom;  	// forward decl
struct io_poller;	// forward decl


/**
 * This routine is called whenever there is action on an atom.
 *
 * @param atom The atom that the proc is being called on.
 * @param flags What sort of action is happening.
 */

typedef void (*io_proc)(struct io_poller *poller, struct io_atom *atom);


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
 *   struct test *test = io_resolve_parent(ap, struct test, a);
 * 
 * TODO: this is totally type unsafe!!  How about using
 * __builtin_types_compatible_p to ensure that *ptr and
 * &result->member are identical?
 */

#define io_resolve_parent(ptr, type, member) \
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
	io_proc read_proc;	///< The function to call when there is a read event on the fd.
	io_proc write_proc;	///< Function to call when there is a write event on the fd.
	int fd;         	///< The fd to watch for events.
};
typedef struct io_atom io_atom;


/** Ensures that you've fully initialized an io_atom.
 * 
 * It's true that initializing an atom is so easy that there's little
 * apparent reason for this macro.  However, if io_atom ever changes
 * (it has in the past), if you use this macro, you're guaranteed to
 * receive error messages where you need to update your code.  If you
 * just bang the fields directly then you'll probably get a partially
 * initialized struct and have to spend a fair amount of debug time
 * trying to figure out what's going on.
 */
#define io_atom_init(io,ff,ppr,ppw) ((io)->fd=(ff),(io)->read_proc=(ppr),(io)->write_proc=(ppw))


/** Reads data from a file or socket.
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

int io_atom_read(struct io_poller *poller, io_atom *io, char *buf, size_t cnt, size_t *len);


/** Writes data to a file or socket
 */

int io_atom_write(struct io_poller *poller, io_atom *io, const char *buf, size_t cnt, size_t *len);

/** Removes the atom from the poller and closes its file or socket.
 */

int io_atom_close(struct io_poller *poller, io_atom *io);


/** Just a utility function.  This routine tries to parse an integer
 *  and, unlike any of the stdc routines, will actually tell you if
 *  an error occurred or not.
 */

int io_safe_atoi(const char *str, int *num);

#endif

