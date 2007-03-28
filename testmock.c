// server.c
// Scott Bronson
// 10 Mar 2007		(based on socktest.c)
//
// shows how to create either a client or a server (or both),
// and how to seamlessly embed mock tests to test them.
//
// Usage: specify one or more of:
//   --client=ADDR   initiate a connection to ADDR
//   --server=ADDR   listen for connections on ADDR
// All connections just echo the data they receive.
// Modify connection_read_proc to change this.
//   --mock=client to run the mock client tests,
//   --mock=server to run the mock server tests.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <values.h>
#include <getopt.h>
#include "poller.h"


// set by the command-line options
int opt_mock_client, opt_mock_server;


#define DEFAULT_PORT 6543


typedef struct {
	io_atom io;
	char c;
	int chars_processed;
} connection;


static const mock_connection listener = {
	"listener", mock_socket, "127.0.0.1:6543"
};

static const mock_connection alan = {
	"alan", mock_socket, "127.0.0.1:49152"
};

static const mock_connection barney = {
	"barney", mock_socket, "127.0.0.1:49153"
};


// client
static const mock_event_queue client_events = {
	MAX_EVENTS_PER_SET, {
	{ 
		// client a requests a connection
		{ EVENT, mock_connect, &alan, "127.0.0.1:6543" },
		{ EVENT, mock_connect, &barney, "127.0.0.1:6543" },
	},
	{
		{ EVENT, mock_event_read, &alan },
		{ EVENT, mock_read, &alan, MOCK_DATA("hi\n") },
		{ EVENT, mock_write, &alan, MOCK_DATA("hi\n") },
		{ EVENT, mock_read, &alan, MOCK_ERROR(EAGAIN) },
	},
	{		
		{ EVENT, mock_event_read, &alan },
		{ EVENT, mock_read, &alan, MOCK_DATA("ho\n") },
		{ EVENT, mock_write, &alan, MOCK_DATA("ho\n") },
		{ EVENT, mock_read, &alan, MOCK_ERROR(EAGAIN) },
		{ EVENT, mock_event_read, &barney },
		{ EVENT, mock_read, &barney, MOCK_DATA("hi, how are you?\n") },
		{ EVENT, mock_write, &barney, MOCK_DATA("hi, how are you?\n") },
		{ EVENT, mock_read, &barney, MOCK_ERROR(EAGAIN) },
	},
	{
		{ EVENT, mock_event_read, &alan },
		{ EVENT, mock_read, &alan, MOCK_DATA("") },	// close alan normally
		{ EVENT, mock_close, &alan },
		
		{ EVENT, mock_event_read, &barney },
		{ EVENT, mock_read, &barney, MOCK_DATA("oh foo.") },
		{ EVENT, mock_write, &barney, MOCK_DATA("oh foo.") },
		{ EVENT, mock_read, &barney, MOCK_ERROR(EAGAIN) },
	},
	{
		{ EVENT, mock_event_read, &barney },
		{ EVENT, mock_read, &barney, MOCK_ERROR(ECONNRESET) },	// barney resets
		{ EVENT, mock_close, &barney },
	},
	{
		{ EVENT, mock_finished }		// null terminator
	}
}};


// TODO: test connection refused.
// TODO: if data == null, then routine returns the value in len as the error.
// TODO: how do I find the size of this queue so I don't have to null-terminate it?
// TODO: we're a little too cavalier when finding events to handle...  If the events
//    specify read FH1, write FH1, but the app checks for the write first, it will
//    find it!  We need to restrict events by connection, not by type.  But that's 
//    a lote more code for a rather unclear gain...  (in other words, it doesn't
//    matter what order read and write events occur in in relation to each other in
//    an event set.  But it should!)

// You can disable an event by setting it to mock_nop.  All other events
// in the set will still be handled.

static const mock_event_queue server_events = {
	MAX_EVENTS_PER_SET, {
	{ // wait 0   (i.e. io_wait hasn't been called yet)
		// TODO: what should we do about the address being duplicated
		// both here and in the connection record itself?  Probably
		// leave it in the conenction record and just ignore this?
		{ EVENT, mock_listen, &listener, "127.0.0.1:6543" },
	},
	{ // wait 1    (io_wait has been called once)
		{ EVENT, mock_event_read, &listener },
		{ EVENT, mock_accept, &alan, "127.0.0.1:6543" },
	},
	{ // wait 2
		{ EVENT, mock_event_read, &alan },
		{ EVENT, mock_read, &alan, MOCK_DATA("hi\n") },
		{ EVENT, mock_write, &alan, MOCK_DATA("hi\n") },
		{ EVENT, mock_read, &alan, MOCK_ERROR(EAGAIN) },
	},
	{ // ...etc.
		{ EVENT, mock_event_read, &alan },
		{ EVENT, mock_read, &alan, MOCK_DATA("ho\n") },
		{ EVENT, mock_write, &alan, MOCK_DATA("ho\n") },
		{ EVENT, mock_read, &alan, MOCK_DATA("hee\n") },
		{ EVENT, mock_write, &alan, MOCK_DATA("hee\n") },
		{ EVENT, mock_read, &alan, MOCK_ERROR(EAGAIN) },
	},
	{
		{ EVENT, mock_event_read, &alan },
		{ EVENT, mock_read, &alan, MOCK_ERROR(EPIPE) },		// close conn
		{ EVENT, mock_close, &alan }
	},
	{
		{ EVENT, mock_finished }		// null terminator
	}
}};



static int echo_data(io_poller *poller, connection *conn, const char *readbuf, size_t rlen, size_t *wlen)
{
	int err;
	
	err = io_write(poller, &conn->io, readbuf, rlen, wlen);
	printf("wrote %d chars to %d\n", (int)*wlen, conn->io.fd);
	conn->chars_processed += *wlen;
	if(rlen < *wlen || err == EAGAIN || err == EWOULDBLOCK) {
					// the remote didn't accept all the data we wrote.
		err = 0; 	// not a fatal error; we'll just drop the data.
					// a real program would enable write events and then
					// wait until it's notified that space is available.
	}
	
	return err;
}


// data is available on the connection
void connection_read_proc(io_poller *poller, io_atom *ioa)
{
	connection *conn = io_resolve_parent(ioa, connection, io);
	char readbuf[1024];
	int err;
    size_t rlen, wlen;
        
	do {
		err = io_read(poller, ioa, readbuf, sizeof(readbuf), &rlen);
		assert(err ? !rlen : 1);
		assert(rlen ? !err : 1);
		if(!err) {
			printf("Received %d bytes on %d, echoing <<<%.*s>>>\n",
					(int)rlen, ioa->fd, (int)rlen, readbuf);
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
}


void connection_write_proc(io_poller *poller, io_atom *ioa)
{
	// When this event arrives it indicates that space in the write
	// buffer has been freed up so we can continue writing.  In this
	// app, though, we just abandon data that can't be written so we
	// never actually turn on write events.
}


// called for every incoming connection request on listening sockets
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

	if(io_accept(poller, &conn->io, connection_read_proc, connection_write_proc, IO_READ, ioa, &remote) < 0) {
		fprintf(stderr, "%s while accepting connection from %s:%d\n",
				 strerror(errno), inet_ntoa(remote.addr), remote.port);
		return;
	}

	printf("Connection opened from %s port %d, given fd %d\n",
		inet_ntoa(remote.addr), remote.port, conn->io.fd);
}


// given an addr:port string, opens an outgoing connection to that host.
void initiate_connection(io_poller *poller, const char *str)
{
    socket_addr remote = { { htonl(INADDR_ANY) }, DEFAULT_PORT };
	connection *conn;
	const char *err;

    conn = malloc(sizeof(connection));
	if(!conn) {
		perror("allocating connection");
		exit(1);
	}

	err = io_parse_address(str, &remote);
	if(err) {
		fprintf(stderr, err, str);
		exit(1);
	}

	if(io_connect(poller, &conn->io, connection_read_proc, connection_write_proc, remote, IO_READ) < 0) {
		fprintf(stderr, "%s while connecting to %s:%d\n",
				 strerror(errno), inet_ntoa(remote.addr), remote.port);
		exit(1);
	}

	printf("Connection opened to %s:%d, given fd %d\n",
		inet_ntoa(remote.addr), remote.port, conn->io.fd);
}


// given an addr:port string, opens a listening socket on that address.
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

	if(io_listen(poller, atom, accept_proc, sock) < 0) {
		perror("listen");
		exit(1);
	}
	
	printf("Opened listening socket on %s:%d, fd=%d\n",
		inet_ntoa(sock.addr), sock.port, atom->fd);
}


static void init_poller(io_poller *poller, io_poller_type type)
{
	if(poller->poller_type != POLLER_NONE) {
		// init_poller has already been called.  Make sure that we don't
		// try to switch from a mock to a non-mock poller or vice versa.
		if( (type != POLLER_MOCK && poller->poller_type != POLLER_MOCK) ||
			(type == POLLER_MOCK && poller->poller_type == POLLER_MOCK)
		) {
			return;	// poller already created, nothing to do.
		}
		
		fprintf(stderr, "You may either run --mock or --client/--server, not both.\n");
		exit(1);
	}
	
	io_poller_init(poller, type);
	if(!poller->poller_name) {
		printf("Could not start the poller!\n");
		exit(1);
	}
	
	printf("Using %s to poll.\n", poller->poller_name);
}


static void process_args(io_poller *poller, int argc, char **argv)
{
    char buf[256], *cp;
    int optidx, i, c;

	optidx = 0;
	static struct option longopts[] = {
		// name, has_arg (1=reqd,2=opt), flag, val
		{"mock", 1, 0, 'm'},
		{"client", 1, 0, 'c'},
		{"server", 1, 0, 's'},
		{0, 0, 0, 0},
	};

	// dynamically create the option string from the long
	// options.  Why oh why doesn't glibc do this for us???
	cp = buf;
	for(i=0; longopts[i].name; i++) {
		if(!longopts[i].flag) {
			*cp++ = longopts[i].val;
			if(longopts[i].has_arg > 0) *cp++ = ':';
			if(longopts[i].has_arg > 1) *cp++ = ':';
		}
	}
	*cp++ = '\0';

	for(;;) {
		c = getopt_long(argc, argv, buf, longopts, &optidx);
		if(c == -1) break;
		
		switch(c) {
        case 'c':
        	init_poller(poller, POLLER_ANY);
        	initiate_connection(poller, optarg);
            break;

		case 'm':
			init_poller(poller, POLLER_MOCK);
			if(optarg[0] == 'c') opt_mock_client++;
			else if(optarg[0] == 's') opt_mock_server++;
			else { fprintf(stderr, "Specify either 'client' or 'server', not '%s'.\n", optarg); exit(1); }
			break;

		case 's':
			init_poller(poller, POLLER_ANY);
			create_listener(poller, optarg);
			break;
			
		case '?':
			// getopt_long already printed the error message
			exit(1);
		}
	}
}


int main(int argc, char **argv)
{
	io_poller poller;
	
	// clear poller_type because init_poller uses it to see if the poller has been initialized.
	poller.poller_type = POLLER_NONE;
	
	// Process the command line arguments (which causes an appropriate poller to be created).
	process_args(&poller, argc, argv);
	if(!poller.poller_name) {
		// poller was not initialized probably because no arguments were specified.
		fprintf(stderr, "You must specify --client, --server or --mock.\n");
		exit(1);
	}

	// If user wants to run mock tests, we need to install them.
	if(opt_mock_client) {
		io_mock_set_events(&poller, &client_events);
		// the test script uses two outgoing connections
		initiate_connection(&poller, "127.0.0.1:6543");
		initiate_connection(&poller, "127.0.0.1:6543");
	} else if(opt_mock_server) {
		io_mock_set_events(&poller, &server_events);
		create_listener(&poller, "127.0.0.1:6543");
	}

	// Run the main event loop.
	for(;;) {
		if(io_wait(&poller, MAXINT) < 0) {
			perror("io_wait");
		}
		io_dispatch(&poller);
	}

	io_poller_dispose(&poller);
	return 0;
}
