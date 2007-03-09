// note: this is experimental and subject to a lot of change.

#include <stdlib.h>
#include <string.h>

#include "readbuf.h"
#include "io_socket.h"


/* io/readbuf.c
 *
 * Maintains a contiguous buffer of read data.  This way the client
 * can let data pool up until it has enough to process in a single
 * chunk.
 *
 * Technically we should probably use a circular buffer for this...
 * Should I pull in the fifo stuff from rzh?
 *
 * This is experimental.  Forcing everything to parse to completion
 * was unweildy (until tools come along that will allow me to create
 * a parser that can save its entire state at any time while parsing).
 * So, this will give us the ability to process things in a more
 * chunky fashion.
 */

int io_readbuf_create(io_readbuf *rb, int bufsiz)
{
	rb->buf = malloc(bufsiz);
	if(rb->buf == NULL) {
		return -1;
	}

	rb->used = 0;
	rb->bufsiz = bufsiz;

	return 0;
}


/** Clears the given number of bytes from the front of the readbuf.
 * Hopefully you can process everything so clearing just resets the
 * data in the buffer.  Until we use ringbuffers, don't clear the
 * first byte off the front of a full 8K buffer -- the computer will
 * thrash copying all the rest of the chars down.
 */

void io_readbuf_clear(io_readbuf *rb, int bytes)
{
	if(bytes >= rb->used) {
		rb->used = 0;
	} else {
		rb->used -= bytes;
		memmove(rb->buf, rb->buf+bytes, rb->used);
	}
}


void io_readbuf_delete(io_readbuf *rb)
{
	free(rb->buf);
}


/** Always always returns the number of bytes to read.  Even if an
 *  ocurred on the last read, it still returns the number of bytes
 *  available in the buffer.
 */

int io_readbuf_read(io_readbuf *rb, io_atom *io)
{
	size_t len;

	if(rb->used >= rb->bufsiz) {
		// no room in the buffer to read!
		return rb->bufsiz;
	}

	rb->err = io_socket_read(io, rb->buf+rb->used, rb->bufsiz-rb->used, &len);
	rb->used += len;

	return rb->used;
}



