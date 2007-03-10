// iotest.c
// Scott Bronson
// 11 Aug 2004
//
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


#if MULTI
#include "pollers/multi.h"
#else
#include "pollers/select.h"
#endif


#define PORT 21314


io_poller poller;
io_atom g_accepter;   // the listening socket
char g_char = 'A';
char g_readbuf[1024];


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
	io_del(&poller, &conn->io);
	close(conn->io.fd);
	conn->io.fd = -1;
	free(conn);
}


void connection_proc(io_atom *ioa, int flags)
{
	connection *conn = (connection*)ioa;
    int fd = conn->io.fd;
    int len;
        
    if(flags & IO_READ) { 
        do {
            len = read(fd, g_readbuf, sizeof(g_readbuf));
        } while (errno == EINTR);   // stupid posix
    
        if(len > 0) {
			write(fd, g_readbuf, len);
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

	conn->io.fd = sd;
	conn->io.proc = connection_proc;

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
		cnt = io_wait(&poller, MAXINT);
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
	
	io_init(&poller);
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

	g_accepter.fd = sd;
	g_accepter.proc = accept_proc;

	if(io_add(&poller, &g_accepter, IO_READ) < 0) {
		perror("io_add_main");
		close(sd);
		exit(1);
	}

	printf("Listening on port %d, fd %d.\n", PORT, g_accepter.fd);
	event_loop();

	io_exit(&poller);

	return 0;
}


