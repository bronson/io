// mock.h
// Scott Bronson
// 22 Mar 2007

#include "../atom.h"


#define MAX_MOCK_FDS 32


struct io_mock_poller {
	int step;
	struct {
		int fd;
		int flags;
		io_atom *atom;
		io_mock_data *events;
	} atoms[MAX_MOCK_FDS];
};
typedef struct io_mock_poller io_mock_poller;


int io_mock_init(io_mock_poller *poller);
int io_mock_poller_dispose(io_mock_poller *poller);
int io_mock_fd_check(io_mock_poller *poller);
int io_mock_add(io_mock_poller *poller, io_atom *atom, int flags);
int io_mock_set(io_mock_poller *poller, io_atom *atom, int flags);
int io_mock_remove(io_mock_poller *poller, io_atom *atom);
int io_mock_wait(io_mock_poller *poller, unsigned int timeout);
int io_mock_dispatch(struct io_poller *poller);

/// Returns the fd of an appropriate unused mock connection.
/// If you've used up all connections, returns -1 and sets errno to EMFILE.
int io_mock_open(io_poller *poller);
void io_mock_init(io_poller *poller, io_atom *atom, io_proc read, io_proc write);

void io_mock_queue_read(io_poller *poller, int fd, char *data, int size);
void io_mock_queue_string(io_poller *poller, int fd, char *data);
void io_mock_queue_write(io_poller *poller, int fd);

