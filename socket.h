/** @file socket.h
 *
 * This layers some IPv4 socket functionality on top of
 * whatever poller you decide to use.
 */

#ifndef IO_SOCKET_H
#define IO_SOCKET_H

#include <netinet/in.h>
#include <arpa/inet.h>

struct io_poller;


struct socket_addr {
	struct in_addr addr;	///< packed address in network order (use inet_ntoa), htonl(INADDR_ANY) to listen on all addresses, or htonl(INADDR_LOOPBACK) to specify the loopback device.
	int port;				///< port number in host order (regular int)
};
typedef struct socket_addr socket_addr;


/// Tells how many incoming connections we can handle at once
/// (this is just the backlog parameter to listen; it's hardly
/// even relevant anymore on Linux).
#ifndef STD_LISTEN_SIZE
#define STD_LISTEN_SIZE 128
#endif


/** Sets up an outgoing connection
 *
 * @param io The io_atom to use.
 * @param proc The io_proc to give the atom.
 * @param remote the IP address and port number of the system to connect to.
 * @param flags The read/write flags that the atom should start with.
 *
 * todo: I don't think we give the user a way to select the local socket?
 * do we?
 */

int io_socket_connect(struct io_poller *poller, io_atom *io, io_proc read_proc, io_proc write_proc, socket_addr remote, int flags);


/** Accepts an incoming connection.
 *
 * You must have previously set up a listening socket using io_socket_listen.
 *
 * @param io       The io_atom to initialize with the new connection.
 * @param proc     The io_proc to initialize the atom with.
 * @param listener The io_atom listening for incoming connections.
 *                 Previously set up using io_socket_listen().
 * @param remote   Returns the IP address and port number of the remote
 *                 system.  Pass NULL if you don't care.
 */

int io_socket_accept(struct io_poller *poller, io_atom *io, io_proc read_proc, io_proc write_proc, int flags, io_atom *listener, socket_addr *remote);


/** Sets up a socket to listen for incoming connections.
 *
 * Connections are passed to the io_proc using IO_READ.
 * (todo: insert example code here)
 *
 * @param io The io_atom to initialize for the new connection.
 * @param proc The io_proc to initialize the atom with.
 * @param the local IP address and port to listen on.  Use INADDR_ANY
 * 		to get the
 */

int io_socket_listen(struct io_poller *poller, io_atom *io, io_proc accept_proc, socket_addr local);


/** Parses a string to an address suitable for use with io_socket.
 *  Accepts "1.1.1.1:22", "1.1.1.1" (default port), and "22" (default
 *  address).  If either the address or port weren't specified, then
 *  this code leaves the original value unchanged.  You can either
 *  fill in defaults before passing sock to this routine, or you can
 *  use sock.addr.s_addr=INADDR_ANY and sock.port=0 for illegal NULL
 *  values.
 *
 *  If the string doesn't specify either an address or a port then the
 *  original contents of the sock variable remain unchanged.
 *
 *  @returns NULL if no error, or an error format string if an error was
 *  discovered.  To handle the error, use something like the following
 *  code:
 *
 *  	char *errstr = io_socket_parse(str, &sock);
 *  	if(errstr) {
 *  		fprintf(stderr, errstr, str);
 *  		exit(1);
 *  	}
 */

char* io_parse_address(const char *spec, socket_addr *sock);

#endif
