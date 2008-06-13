/** @file io_socket.c
 *
 * This is actually a layer that adds sockets to whatever underlying io
 * layer you decide to use.
 */

#include "io.h"


struct in_addr;

int io_accept(io_atom *io, struct in_addr *remoteaddr, int *remoteport);
int io_listen(io_atom *io, struct in_addr addr, int port);
ssize_t io_read(io_atom *io, char *buf, size_t cnt);

