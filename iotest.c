// iotest.c
// Scott Bronson
// 11 Aug 2004
//
// Run this program then "telnet localhost 21314" a bunch of times.
// It just echoes each socket's input back.
//
// You should look at the testserver.c demo.  It does more than this
// example in significantly less code because it uses io/socket.c.
// If you want to see how to interface with IO Atom at the low level,
// however, this demo might be interesting.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "poller.h"

// The port we should listen on
#define PORT 21314

// Sometimes this is defined for you, sometimes not.
#ifndef STD_LISTEN_SIZE
#define STD_LISTEN_SIZE 128
#endif


io_poller poller;
io_atom g_accepter;   // the listening socket
char g_char = 'A';
char readbuf[1024];


typedef struct {
	io_atom io;
	char c;
	int chars_processed;
} connection;


int set_nonblock(int sd)
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


void connection_close(connection *conn)
{
	io_close(&poller, &conn->io);
	free(conn);
}


void connection_read_proc(io_poller *pp, io_atom *ioa)
{
	connection *conn = io_resolve_parent(ioa, connection, io);
    int fd = conn->io.fd;
    int len;
    
    do {
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
    } while(len > 0);
}


void connection_write_proc(io_poller *pp, io_atom *ioa)
{
	// there's more space in the write buffer
	// so continue writing.  (this is a little complex; not demonstrated)
}


void accept_proc(io_poller *pp, io_atom *ioa)
{
    connection *conn;
    struct sockaddr_in pin;
    socklen_t plen;
    int sd;

	// since the accepter only has IO_READ anyway, there's no need to
	// check the flags param.

    plen = sizeof(pin);
    while((sd = accept(ioa->fd, (struct sockaddr*)&pin, &plen)) < 0) {
        if(errno == EINTR) {
            // This call was interrupted by a signal.  Try again and
            // see if we receive a connection.
            continue;
        }
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            // socket is marked nonblocking but no pending connections
            // are present.  Weird.  I guess we should succeed but do nothing.
            return;
        }

        // Probably there is a network error pending for this
        // connection (already!).  Should probably just ignore it...?
        return;
    }

    if(set_nonblock(sd) < 0) {
        printf("Could not set nonblocking: %s.\n", strerror(errno));
        close(sd);
        return;
    }

    conn = malloc(sizeof(connection));
    if(!conn) {
		close(sd);
		return;
	}

    io_atom_init(&conn->io, sd, connection_read_proc, connection_write_proc);
	if(io_add(&poller, &conn->io, IO_READ) < 0) {
		perror("io_add_main");
		close(sd);
		exit(1);
	}

	printf("connection opened from %s port %d given fd %d\n",
		inet_ntoa(pin.sin_addr), ntohs(pin.sin_port), conn->io.fd);
}


void event_loop()
{
	int cnt;
	
	for(;;) {
		cnt = io_wait(&poller, INT_MAX);
		if(cnt < 0) {
			perror("io_wait");
		}
		if(cnt > 0) {
			io_dispatch(&poller);
		}
	}
}


int main(int argc, char **argv)
{
	int sd;
	struct sockaddr_in sin;
	
	io_poller_init(&poller, IO_POLLER_ANY);
	if(!poller.poller_name) {
		printf("Could not start a poller!\n");
		exit(1);
	}
	
	printf("Opening listening socket...\n");

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
	sin.sin_port = htons(PORT);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(sd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
		perror("bind");
		close(sd);
		exit(1);
	}

	if (listen(sd, STD_LISTEN_SIZE) == -1) {
		perror("listen");
		close(sd);
		exit(1);
	}

	io_atom_init(&g_accepter, sd, accept_proc, NULL);
	
	if(io_add(&poller, &g_accepter, IO_READ) < 0) {
		perror("io_add_main");
		close(sd);
		exit(1);
	}

	printf("Listening on port %d, fd %d.\n", PORT, g_accepter.fd);
	event_loop();

	io_poller_dispose(&poller);

	return 0;
}


