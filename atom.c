// atom.c
// Scott Bronson
// 22 Feb 2006


// Some utility functions that are common to all IO Atom pollers.

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "atom.h"


/** Reads from the given io_atom.
 *
 * @param io The io_atom to read from.
 * @param buf the buffer to fill with data
 * @param cnt the number of bytes to read
 * @param readlen returns the number of bytes you need to process.
 * If readlen is nonzero, then error is guaranteed to be 0.
 *
 * @returns 0 on success, or the error code in errno if there was
 * an error.  If there was an error but errno doesn't say what it was
 * then -1 is returned.  A return value of EPIPE means you hit EOF
 * (file) or the remote peer closed its connection (pipe, socket).
 * If the remote reset its connection, we return EPIPE as well
 * (because, like close, this indicates that the remote has disappeared).
 */

int io_read(io_atom *io, char *buf, size_t cnt, size_t *readlen)
{
    ssize_t len;

	*readlen = 0;
    do {
        len = read(io->fd, buf, cnt);
    } while (errno == EINTR);   // stupid posix

    if(len > 0) {
        // success!
		*readlen = len;
        return 0;
	}

    if(len == 0) {
		// the remote has closed the connection.
		return EPIPE;
    }

    // an error ocurred during the read
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
        // turns out there was nothing to read after all?  weird.
        // there was no error, but nothing to process either.
        return 0;
    } 

    // Not sure this is a good idea... ECONNRESET means that the
    // peer has closed the connection (probably by suddenly losing
    // power rather than simply closing the socket) so we'll treat
    // it the same as the remote shutting down.
    if(errno == ECONNRESET) {
        return EPIPE;
    }

    return errno ? errno : -1;
}


/**
 * Writes to the given atom.
 *
 * You should always be able to write all data.
 *
 * @returns 0 on success, the error code if there was a failure.
 *
 * A return value of -EPIPE means that the remote peer closed its
 * connection.  HOWEVER, your app will already have received a
 * SIGPIPE, so you'll have to block this signal if you want to
 * handle the -EPIPE return code.
 *
 * We treat handle the remote resetting the connection the same as
 * it closing the connection.  So, when errno is ECONNRESET, this
 * routine will return EPIPE to tell you that the remote is gone.
 * You don't need to worry about this: EPIPE means no more remote.
 */

int io_write(io_atom *io, char *buf, size_t cnt, size_t *wrlen)
{
    ssize_t len;

    *wrlen = 0;
    do {
        len = write(io->fd, buf, cnt);
    } while(errno == EINTR);

    if(len > 0) {
        *wrlen = len;
        return 0;
    } 

    if(len < 0) {
        if(errno == ECONNRESET) {
            return EPIPE;
        }
        return errno ? errno : -1;
    }

    // nothing was written but there was no error??
    return 0;
}


/** Removes the atom from polling and closes its file descriptor.
 */

void io_close(io_atom *io)
{
	io_del(io);
	close(io->fd);
	io->fd = -1;
}


/** 
 * Converts a string into an integer.
 * 
 * It is unfortunate that this routine has to be included
 * in the core library but I don't know where else to put
 * it...
 * 
 * Unlinke the standard C routines,
 * this routine returns an error if the conversion fails.
 * C generally just gives you a garbage number.
 * 
 * Also, unlinke the standard routine, the string must be
 * null terminated and contain only the number.
 * 
 * @returns 1 if the conversion was successful, 0 if not.
 */

int io_safe_atoi(const char *str, int *num)
{
	char *remainder;

	if(*str == '\0') {
		// blank string (strtol would parse this as "0").
		return 0;
	}

	errno = 0;
	*num = strtol(str, &remainder, 10);
	if(errno) {
		return 0;
	}

	if(*remainder != '\0') {
		return 0;
	}

	return 1;
}
