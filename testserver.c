// server.c
// Scott Bronson
// 10 Mar 2007		(based on socktest.c)
//
// Listens on the ports you specify, echoes data back at each socket
// that connects.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <values.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "poller.h"


#define DEFAULT_PORT 6543


typedef struct {
	io_atom io;
	char c;
	int chars_processed;
} connection;


int echo_data(io_poller *poller, connection *conn, const char *readbuf, size_t rlen, size_t *wlen)
{
	int err;
	
	err = io_write(poller, &conn->io, readbuf, rlen, wlen);
	printf("wrote %d chars to %d\n", (int)*wlen, conn->io.fd);
	conn->chars_processed += *wlen;
	if(rlen < *wlen || err == EAGAIN || err == EWOULDBLOCK) {
		// When we can't perform a full write, it means that
		// the remote isn't accepting data fast enough.  To
		// handle this situation correctly, we would have to
		// hang onto the unwritten data until an IO_WRITE
		// event tells us that there's more room for us to
		// finish the write.  This is an exercise left for
		// the reader.  :)  (it's not a hard fix, but it
		// does require a per-connection buffer to store
		// the unwritten data).

		err = 0; // not a fatal error; we'll just drop the data.
		// Note that even though we can't write, we continue
		// reading until the input buffer is exhausted.
	}
	
	return err;
}


void connection_read_proc(io_poller *poller, io_atom *ioa)
{
	connection *conn = io_resolve_parent(ioa, connection, io);
	char readbuf[1024];
	int err;
    size_t rlen, wlen;
        
	do {
		err = io_read(poller, ioa, readbuf, sizeof(readbuf), &rlen);
		if(!err) {
			err = echo_data(poller, conn, readbuf, rlen, &wlen);
			if(err) break;
		}
	} while(rlen);
	
	// read and write errors both end up here.  EAGAIN and EWOULDBLOCK
	// are not errors -- they're a normal part of non-blocking I/O.
	if(err && err != EAGAIN && err != EWOULDBLOCK) {
		if(err == EPIPE || err == ECONNRESET) {
			printf("connection closed by remote on fd %d\n",
				conn->io.fd);
		} else {
			printf("error %s on fd %d, closing!\n", strerror(errno),
				conn->io.fd);
		}

		// close the connection, free its memory
		io_close(poller, &conn->io);
		free(conn);
	}

	// It's true, we perform at least two reads every time data is
	// available!  Doesn't these extra system calls hurt performance?
	//
	// Well, POSIX is unfortunate...  They allow an interrupted read
	// or write to return either EINTR and 0 bytes (that's OK by me)
	// OR return no error and some partial amount of data.  Therefore
	// you can't loop on while(len == sizeof(readbuf)) because while,
	// in theory, another read after a partial read should
	// always return EAGAIN), a signal could cause
	// the loop to exit before you've exhausted the input buffer.
	// Tou will receive no more IO_READ events until more data arrives
	// (which may not happen if the remote is waiting for you to
	// to send a reply to the data that's already in the input buffer).
	// And your connection will hang.
	//
	// SUMMARY: just keep reading until you get EAGAIN or EWOULDBLOCK
	// (those // are the same on Linux).  Don't try to optimize the
	// redundant read away.
	//
	// I think the solution is to perform a single read into a gigantic
	// buffer each time through the event loop.  If the read returned
	// data, then you mark that fd to be explicitly read the next time
	// through the event loop (probably after all other fds are handled).
	// Just swap the fd in and out of a doubly linked list.
}


void connection_write_proc(io_poller *poller, io_atom *ioa)
{
	// When this event arrives it indicates that space in the write
	// buffer has been freed up so continue writing.
}


// called for every incoming connection request
void accept_proc(io_poller *poller, io_atom *ioa)
{
	connection *conn;
	socket_addr remote;

	// since the accepter only has IO_READ anyway, there's no need to
	// check the flags param.

    conn = malloc(sizeof(connection));
	if(!conn) {
		perror("allocating connection");
		return;
	}

	if(io_accept(poller, &conn->io, connection_read_proc, connection_write_proc, IO_READ, ioa, &remote)) {
		perror("connecting to remote");
		return;
	}

	printf("Connection opened from %s port %d, given fd %d\n",
		inet_ntoa(remote.addr), remote.port, conn->io.fd);
}


// given an addr:port string, opens a listening socket
void create_listener(io_poller *poller, const char *str)
{
	io_atom *atom;
	socket_addr sock = { { htonl(INADDR_ANY) }, DEFAULT_PORT };
	const char *err;

	atom = malloc(sizeof(io_atom));
	if(!atom) {
		perror("malloc");
		exit(1);
	}

	// if a string was supplied, we use it, else we just
	// use the defaults that are already stored in sock.
	if(str) {
		err = io_parse_address(str, &sock);
		if(err) {
			fprintf(stderr, err, str);
			exit(1);
		}
	}

	if(io_listen(poller, atom, accept_proc, sock)) {
		perror("listen");
		exit(1);
	}
	
	printf("Opened listening socket on %s:%d, fd=%d\n",
		inet_ntoa(sock.addr), sock.port, atom->fd);
}


int main(int argc, char **argv)
{
	io_poller poller;
	
	io_poller_init(&poller, IO_POLLER_ANY);
	if(!poller.poller_name) {
		printf("Could not start a poller!\n");
		exit(1);
	}

	printf("Using %s to poll.\n", poller.poller_name);

	if(argc == 1) {
		// if no cmdline args, create default listener
		create_listener(&poller, NULL);
	}
	for(argv++; *argv; argv++) {
		// open one listener for every argument specified.
		create_listener(&poller, *argv);
	}

	for(;;) {
		if(io_wait(&poller, MAXINT) < 0) {
			perror("io_wait");
		}
		io_dispatch(&poller);
	}

	io_poller_dispose(&poller);
	return 0;
}


