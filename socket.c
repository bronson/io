/** @file socket.c
 *
 * This file adds socket functionality to whatever underlying io
 * layer you decide to use (epoll, select, etc).
 *
 * CHANGE!!  We now return EPIPE to indicate that the remote
 * closed the connection (instead of -EPIPE).  The whole -errno
 * hack has been depricated -- you should never again see negative
 * errno numbers.
 */

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
#include <netdb.h>

#include "poller.h"


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


/** Connects to the given address.
 * 
 *  @param remote The address to connect to.
 *  @returns the new socket fd (>=0) or the error (<0) if unsuccessful.
 */

static int connect_fd(socket_addr remote)
{
    struct sockaddr_in sa;
    int err;
	int fd;
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
		return fd;
    }

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(0);
    
	err = bind(fd, (struct sockaddr*)&sa, sizeof(sa));
	if(err < 0) {
		goto bail;
	}

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr = remote.addr;
    sa.sin_port = htons(remote.port);
    
    err = connect(fd, (struct sockaddr*)&sa, sizeof(sa));
    if(err < 0) {
		goto bail;
    }
    
    if(set_nonblock(fd) < 0) {
		goto bail;
    }

	return fd;

bail:
	close(fd);
	return -1;
}


/** Creates an outgoing connection.
 *
 * @param io A totally uninitialized io_atom.  This call will fill everything in.
 * @param remote The address you want to connect to.
 * @param port The port you want to connect to.
 * @param io_proc The io_proc that you want this atom to use.
 * @param flags The flags that the atom should have initially.
 * 	(IO_READ will set it up initially to watch for read events,
 * 	IO_WRITE for write events).
 * 
 * @returns The new fd (>=0) if successful, or an error (<0) if not.
 * If there was an error, you should find the reason in strerror.
 */

int io_socket_connect(io_poller *poller, io_atom *io, io_proc read_proc, io_proc write_proc, socket_addr remote, int flags)
{   
	int err;

	io->fd = connect_fd(remote);
	if(io->fd < 0) {
		return -1;
	}

	io_atom_init(io, io->fd, read_proc, write_proc);
	err = io_add(poller, io, flags);
    if(err < 0) {
		goto bail;
    }

    return io->fd;

bail:
	close(io->fd);
	io->fd = -1;
	return -1;
}


/** Accepts an incoming connection.
 *
 * You should first set up a listening socket using io_listen.
 * The event proc on the listening socket should call io_accept
 * every time it gets a connection request.
 *
 * Note that this only returns the fd.  If you want to use io_atoms
 * on the connected socket, you must set up the new atom yourself.
 * This way, you're not locked into using atoms to read even if
 * you're using atoms to listen.
 *
 * @param io The socket that the incoming connection is arriving on.
 * @param remoteaddr If specified, store the address of the remote
 * computer initiating the connection here.  NULL means ignore.
 * @param remoteport If specified, store the port of the remote
 * computer initiating the connection here.  NULL means ignore.
 * @returns the new socket descriptor if we succeeded or -1 if
 * there was an error.
 */

int io_socket_accept(io_poller *poller, io_atom *io, io_proc read_proc, io_proc write_proc, int flags, io_atom *listener, socket_addr *remote)
{
    struct sockaddr_in pin;
    socklen_t plen = sizeof(pin);
	int err;

    while((io->fd = accept(listener->fd, (struct sockaddr*)&pin, &plen)) < 0) {
        if(errno == EINTR) {
            // This call was interrupted by a signal.  Try again and
            // see if we receive a connection.
            continue;
        }
        return errno ? errno : -1;
    }

    // Excellent.  We managed to connect to the remote socket.
    if(set_nonblock(io->fd) < 0) {
        close(io->fd);
        return errno ? errno : -1;
    }

	io_atom_init(io, io->fd, read_proc, write_proc);
    err = io_add(poller, io, flags);
	if(err < 0) {
        close(io->fd);
        return err;
    }

    if(remote) {
        remote->addr = pin.sin_addr;
        remote->port = (int)ntohs(pin.sin_port);
    }

    return 0;
}


/** Sets up a socket to listen on the given port.
 *
 * @param atom This should the uninitialized atom that will handle the events on
 * the socket. 
 *
 * @returns the new fd.
 */

int io_socket_listen(io_poller *poller, io_atom *io, io_proc read_proc, socket_addr local)
{
    struct sockaddr_in sin;

    if((io->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return -1;
    }

    if(set_nonblock(io->fd) < 0) {
        close(io->fd);
		return -1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr = local.addr;
    sin.sin_port = htons(local.port);

    if(bind(io->fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        close(io->fd);
		return -1;
    }

    // NOTE: if we're dropping connection requests under load, we
    // may need to tune the second parameter to listen.
    if (listen(io->fd, STD_LISTEN_SIZE) == -1) {
        close(io->fd);
		return -1;
    }

    io_atom_init(io, io->fd, read_proc, NULL);
    if(io_add(poller, io, IO_READ) < 0) {
        close(io->fd);
		return -1;
    }

	return io->fd;
}


/** Parses a string to an address suitable for use with io_socket.
 *  Accepts "1.1.1.1:22", "1.1.1.1" (default port), and "22" (default
 *  address).  Also accepts "host:22" and "host".  If a hostname consists
 *  of all numbers (talk about archaic) the it will be interpreted as
 *  a port unless you specify it as "222:".  Uses gethostbyname.
 *
 *  If the string doesn't specify either an address or a port then the
 *  original contents of the sock variable remain unchanged.
 *
 *  @returns NULL if no error, or an error string if an error was
 *  discovered.  The error string is meant to be used as a format
 *  string with spec passed as the single argument, i.e.
 * 
 *     result = io_parse_address(spec, sock);
 *     if(result) {
 *         fprintf(stderr, result, spec);
 *     }
 * 
 * TODO: this error reporting by returning a format string flat out sucks.
 * Fix it!!  Maybe make this routine return an int like all others that
 * can be passed to a strerror-alike routine to produce a readable string?
 */

char* io_parse_address(const char *spec, socket_addr *sock)
{
	char buf[512];
	const char *colon;
	int i;

	struct hostent *he;

	/* If it contains ':' then both an address and a port. */
	colon = strchr(spec, ':');
	if(colon) {
		// it's an address:port combination
		if(colon - spec > sizeof(buf)-1) {
			return "Address is too long: \"%s\"\n";
		}

		// copy the address portion to a separate buffer
		memcpy(buf, spec, colon-spec);
		buf[colon-spec]='\0';

		// port is at colon+1
		spec = colon+1;
	} else {
		// if it parses as a number, it's a lone port and we're done.
		if(io_safe_atoi(spec, &i)) {
			sock->port = i;
			return NULL;
		}

		// it's a lone address
		if(strlen(spec) > sizeof(buf)-1) {
			return "Address is too long: \"%s\"\n";
		}
		strcpy(buf, spec);
		spec = NULL;
	}

	// address is in buf
	if(buf[0]) {
		he = gethostbyname(buf);
		if(he == NULL) {
			if(h_errno == HOST_NOT_FOUND) {
				return "Host \"%s\" not found.\n";
			}
			if(h_errno == NO_ADDRESS || h_errno == NO_DATA) {
				return "No address for host \"%s\".\n";
			}
			return "Error resolving host \"%s\".\n";
		}
		sock->addr = *(struct in_addr*)(he->h_addr_list[0]);
	}

	// port is in spec.  Spec may be empty, in which case
	// we need to be sure to not modify port.
	if(spec && spec[0]) {
		if(io_safe_atoi(spec, &i)) {
			sock->port = i;
		} else {
			return "Invalid port \"%s\" specified.\n";
		}
	}

	return NULL;
}

