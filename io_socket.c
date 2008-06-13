/** @file io_socket.c
 *
 * This is actually a layer that adds sockets to whatever underlying io
 * layer you decide to use.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <values.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>


#include "io_socket.h"


static int set_nonblock(int sd)
{
    int flags = 1;

    // set nonblocking IO on the socket
    if(ioctl(sd, FIONBIO, &flags)) {
        // ioctl failed -- try alternative
        flags = fcntl(sd, F_GETFL, 0);
        if(flags < 0) {
            return -1;
        }
        if(fcntl(sd, F_SETFL, flags | O_NONBLOCK) < 0) {
            return -1;
        }
    }

    return 0;
}


/** Accepts an incoming connection.
 *
 * You should first set up a listening socket using io_listen.
 * The event proc on the listening socket should call io_accept
 * every time it gets a connection request.
 *
 * @param remoteaddr If specified, store the address of the remote
 * computer initiating the connection here.  NULL means ignore.
 * @param remoteport If specified, store the port of the remote
 * computer initiating the connection here.  NULL means ignore.
 * @returns the new socket descriptor if we succeeded or -1 if
 * there was an error.
 */

int io_accept(io_atom *io, struct in_addr *remoteaddr, int *remoteport)
{
    struct sockaddr_in pin;
    socklen_t plen;
    int sd;

    plen = sizeof(pin);
    while((sd = accept(io->fd, (struct sockaddr*)&pin, &plen)) < 0) {
        if(errno == EINTR) {
            // This call was interrupted by a signal.  Try again and
            // see if we receive a connection.
            continue;
        }
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            // socket is marked nonblocking but no pending connections
            // are present.  Weird.  I guess we should succeed but do nothing.
            return -1;
        }

        // Probably there is a network error pending for this
        // connection (already!).  Should probably just ignore it...?
        return -1;
    }

    // Excellent.  We managed to connect to the remote socket.
    if(set_nonblock(sd) < 0) {
        printf("Could not set nonblocking for incoming connection from \"%s:%d\" : %s.  fd was %d\n",
            inet_ntoa(pin.sin_addr), (int)ntohs(pin.sin_port), strerror(errno), sd);
        close(sd);
        return -1;
    }

    if(remoteaddr) {
        *remoteaddr = pin.sin_addr;
    }
    if(remoteport) {
        *remoteport = (int)ntohs(pin.sin_port);
    }

    return sd;
}


/** Sets up a socket to listen on the given port.
 *
 * @param atom This should be the atom that will handle the events on
 * the socket.  It must be pre-allocated, and live for as long as the
 * socket will be listening.  You must fill in io_atom::proc before
 * calling this routine.  io_atom::fd will be filled in with the new
 * fd or -1 if there was an error.
 *
 * @returns the new fd.
 *
 * TODO: this routine should return the error.  It shouldn't just exit.
 */

int io_listen(io_atom *io, struct in_addr addr, int port)
{
    int sd;
    struct sockaddr_in sin;

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    if(set_nonblock(sd) < 0) {
        perror("setnonblocking");
        close(sd);
        exit(1);
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr = addr;

    if(bind(sd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("bind");
        close(sd);
        exit(1);
    }

    // NOTE: if we're dropping connection requests under load, we
    // may need to tune the second parameter to listen.
    if (listen(sd, STD_LISTEN_SIZE) == -1) {
        perror("listen");
        close(sd);
        exit(1);
    }

    io->fd = sd;
    assert(io->proc);

    if(io_add(io, IO_READ) < 0) {
        perror("io_add");
        close(sd);
        exit(1);
    }

	return io->fd;
}


/** Reads from the given io_atom (in response to an IO_READ event).
 *
 * @returns the error code from the read.  It can be:
 * - err > 0: a good read.  the value gives the number of bytes.
 * - err == 0: there was actually no data to read (some sort of fd screwup?)
 * - err == -ECHILD: the remote has closed the connection normally.
 * - err < 0: there was some sort of error.
 *
 * In summary, if err >= 0, you should handle the number of bytes read.
 * if err < 0, you should handle the error (this usually means just close
 * your socket).
 */

ssize_t io_read(io_atom *io, char *buf, size_t cnt)
{
    ssize_t len;

    do {
        len = read(io->fd, buf, cnt);
    } while (errno == EINTR);   // stupid posix

    if(len > 0) {
        // a good read of len bytes.
        return len;
    } else if(len == 0) {
        // remote has closed normally
        return -ECHILD;
    } else {
        // an error ocurred during the read
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            // turns out there was nothing to read after all?  weird.
            assert(!"Got an event but there was nothing to read!");
            return 0;
        } 

        // there's some sort of error on this socket.
        return len;
    }
}
