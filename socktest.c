// socktest.c
// Scott Bronson
// 10 Mar 2007
//
// This is just iotest.c ported to use the io/socket utilities.
// Run this program then "telnet localhost 21314" a bunch of times.
// It just echoes each socket's input back.
//
// This example doesn't really show off asynchronous I/O, the whole point
// of the io_atom library.  And it's doesn't use io/socket.c.
// So, really, it's a pretty darn poor demo.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <values.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "socket.h"


#define PORT 21314


io_poller poller;
io_atom accepter;   // the listening socket


typedef struct {
	io_atom io;
	char c;
	int chars_processed;
} connection;


void connection_close(connection *conn)
{
	io_remove(&poller, &conn->io);
	close(conn->io.fd);
	conn->io.fd = -1;
	free(conn);
}


void connection_proc(io_poller *pp, io_atom *ioa, int flags)
{
	connection *conn = io_resolve_parent(ioa, connection, io);
	char readbuf[1024];
	int err;
    size_t rlen, wlen;
        
    if(flags & IO_READ) { 
		do {
			err = io_read(ioa, readbuf, sizeof(readbuf), &rlen);
			if(!err) {
				err = io_write(ioa, readbuf, rlen, &wlen);
				conn->chars_processed += wlen;
				if(err == EAGAIN || err == EWOULDBLOCK) {
					// When we can't perform a full write, it means that
					// the remote isn't accepting data fast enough.  To
					// handle this situation correctly, we would have to
					// hang onto the unwritten data until an IO_WRITE
					// event tells us that there's more room for us to
					// finish the write.  This is an exercise left for
					// the reader.  :)  (it's not a hard fix, but it
					// does require a per-connection buffer to store
					// unwritten data).

					err = 0; // not a fatal error; we'll just drop the data.
					// Note that even though we can't write, we continue
					// reading until the input buffer is exhausted.
				}
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
				printf("read error %s on fd %d\n", strerror(errno),
					conn->io.fd);
			}
			connection_close(conn);
		}

		// Er, we perform at least two reads every time data is
		// available!
		//
		// Well, POSIX is unfortunate...  They allow an interrupted read
		// or write to return either EINTR and 0 bytes (that's OK by me)
		// OR return no error and some partial amount of data.  Therefore
		// you can NOT loop on while(len == sizeof(readbuf)).  While
		// looping like this will always result in 1 less system call
		// (because in theory, another read after a partial read should
		// always return EAGAIN), it's incorrect.  A signal can cause
		// the loop to exit before you've exhausted the input buffer and
		// you will receive no more IO_READ events until more data arrives
		// (which may not happen if the remote is waiting for you to
		// to send a reply to the data that's already in the input buffer).
		//
		// SUMMARY: just keep reading until you get EAGAIN or EWOULDBLOCK
		// (those // are the same on Linux).  Don't try to optimize the
		// redundant read away.
    }
    
    if(flags & IO_WRITE) {
		// there's more space in the write buffer
		// so continue writing.  We don't demonstrate this.
    }   
            
    if(flags & IO_EXCEPT) {
        // I think this is also used for OOB.
        // recv (fd1, &c, 1, MSG_OOB);
		printf("exception on fd %d\n", conn->io.fd);
		connection_close(conn);
		return;
    }
}


void accept_proc(io_poller *pp, io_atom *ioa, int flags)
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

	if(io_socket_accept(&poller, &conn->io, connection_proc, IO_READ, &accepter, &remote) < 0) {
		perror("connecting to remote");
		return;
	}

	printf("connection opened from %s port %d given fd %d\n",
		inet_ntoa(remote.addr), remote.port, conn->io.fd);
}


int main(int argc, char **argv)
{
	const socket_addr addr = { { htonl(INADDR_ANY) }, PORT };

	io_poller_init(&poller);
	printf("Opening listening socket...\n");

	if(io_socket_listen(&poller, &accepter, accept_proc, addr) < 0) {
		perror("listen");
		exit(1);
	}

	printf("Listening on port %d, fd %d.\n", addr.port, accepter.fd);

	for(;;) {
		if(io_wait(&poller, MAXINT) < 0) {
			perror("io_wait");
		}
		io_dispatch(&poller);
	}

	io_remove(&poller, &accepter);
	io_poller_dispose(&poller);

	return 0;
}


