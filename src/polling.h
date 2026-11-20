#ifndef POLLING_H
#define POLLING_H

typedef enum poll_types_t{
	PT_POLL_UPSTREAM = 0,
	PT_THREAD_TIMER,
	PT_LISTENER,
	PT_CLIENT_CONN,
	PT_IDLE_CLIENT_CHECK,
} poll_types_t;

typedef struct epoll_generic_t{
	enum poll_types_t type;
	int fd;
} epoll_generic_t;

int epoll_add_fd(int efd, epoll_generic_t *generic, int events);
void epoll_remove_fd(int efd, epoll_generic_t *generic);

#endif
