// Note: this is experimental and subject to a lot of change.

#include "io.h"


typedef struct {
	int used;		///< how many bytes are currently in the buffer
	int bufsiz;		///< the size of the buffer
	int err;		///< the error from the last read operation
	char *buf;		///< the buffer.
} io_readbuf;


int io_readbuf_create(io_readbuf *rb, int bufsiz);
void io_readbuf_delete(io_readbuf *readbuf);

int io_readbuf_read(io_readbuf *readbuf, io_atom *io);
void io_readbuf_clear(io_readbuf *rb, int bytes);


