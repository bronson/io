// client.c
// Scott Bronson
// 10 Mar 2007		(based on socktest.c)
//
// Opens connections to the ports you specify, prints what it finds.


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

#include "socket.h"


#define DEFAULT_PORT 6543


typedef struct {
	io_atom io;
	char c;
	int chars_processed;
} connection;


void connection_read_proc(io_poller *poller, io_atom *ioa)
{
	connection *conn = io_resolve_parent(ioa, connection, io);
	char readbuf[1024];
	int err;
    size_t rlen;

    // drain the fd of all data to read
	do {
		err = io_read(ioa, readbuf, sizeof(readbuf), &rlen);
		if(err) break;
		printf("Received %d bytes: <<<%.*s>>>\n", (int)rlen, (int)rlen, readbuf);
	} while(rlen);
	
	// read and write errors both end up here.  EAGAIN and EWOULDBLOCK
	// are not errors -- they're a normal part of non-blocking I/O.
	if(err && err != EAGAIN && err != EWOULDBLOCK) {
		if(err == EPIPE) {
			printf("connection closed by remote on fd %d\n", conn->io.fd);
		} else if(err == ECONNRESET) {
			printf("connection reset by remote on fd %d\n", conn->io.fd);
		} else {
			printf("error %s on fd %d, closing!\n", strerror(errno), conn->io.fd);
		}

		// error?  close the connection, free its memory
		io_remove(poller, &conn->io);
		close(conn->io.fd);
		conn->io.fd = -1;
		free(conn);
	}
}


void connection_write_proc(io_poller *poller, io_atom *ioa)
{
	// This event indicates that space in the write buffer has been
	// freed up.  You can now continue writing to this fd.
}


void create_connection(io_poller *poller, const char *str)
{
    socket_addr remote = { { htonl(INADDR_ANY) }, DEFAULT_PORT };
	connection *conn;
	const char *err;

    conn = malloc(sizeof(connection));
	if(!conn) {
		perror("allocating connection");
		exit(1);
	}

	err = io_socket_parse(str, &remote);
	if(err) {
		fprintf(stderr, err, str);
		exit(1);
	}

	if(io_socket_connect(poller, &conn->io, connection_read_proc, connection_write_proc, remote, IO_READ) < 0) {
		perror("connecting to remote");
		exit(1);
	}

	printf("Connection opened to %s:%d, given fd %d\n",
		inet_ntoa(remote.addr), remote.port, conn->io.fd);
}


int main(int argc, char **argv)
{
	io_poller poller;

	io_poller_init(&poller, POLLER_ANY);
	if(!poller.poller_name) {
		printf("Could not start a poller!\n");
		exit(1);
	}

	printf("Using %s to poll.\n", poller.poller_name);

	if(argc == 1) {
		printf("Specify where to connect, addr:port!\n");
		exit(1);
	}

	// open all the connections found on the command line.
	for(argv++; *argv; argv++) {
		create_connection(&poller, *argv);
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


