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
	io_delete(&poller, &conn->io);
	close(conn->io.fd);
	conn->io.fd = -1;
	free(conn);
}


void connection_proc(io_atom *ioa, int flags)
{
	char readbuf[1024];
	connection *conn = (connection*)ioa;
    int fd = conn->io.fd;
    int len;
        
    if(flags & IO_READ) { 
        do {
            len = read(fd, readbuf, sizeof(readbuf));
        } while (errno == EINTR);   // stupid posix
    
        if(len > 0) {
			write(fd, readbuf, len);
            conn->chars_processed += len;
        } else if(len == 0) {
            // A 0-length read means remote has closed normally
			printf("connection closed by remote on fd %d\n", conn->io.fd);
			connection_close(conn);
			return;
        } else {
            // handle an error on the socket
            if(errno == EAGAIN) {
                // nothing to read?  weird.
            } else if(errno == EWOULDBLOCK) {
                // with glibc EAGAIN==EWOULDBLOCK so this is probably dead code
            } else {
				printf("read error %s on fd %d\n", strerror(errno), conn->io.fd);
				connection_close(conn);
                return;
            }
        }
    }
    
    if(flags & IO_WRITE) {
		// there's more space in the write buffer
		// so continue writing.
    }   
            
    if(flags & IO_EXCEPT) {
        // I think this is also used for OOB.
        // recv (fd1, &c, 1, MSG_OOB);
		printf("exception on fd %d\n", conn->io.fd);
		connection_close(conn);
		return;
    }
}


void accept_proc(io_atom *ioa, int flags)
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
	const socket_addr addr = { { htonl(INADDR_ANY) }, 21314 };

	io_init(&poller);
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

	io_delete(&poller, &accepter);
	io_exit(&poller);

	return 0;
}


