#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include "polling.h"

int epoll_add_fd(int efd, epoll_generic_t *generic, int events){
	struct epoll_event event = {0};
	event.events = events;
	event.data.ptr = generic;

	return epoll_ctl(efd, EPOLL_CTL_ADD, generic->fd, &event);
}

void epoll_remove_fd(int efd, epoll_generic_t *generic){
	struct epoll_event see_bugs_below = {0};

	epoll_ctl(efd, EPOLL_CTL_DEL, generic->fd, &see_bugs_below);
}

int epoll_mod_fd(int efd, epoll_generic_t *generic, int events){
	struct epoll_event event = {0};
	event.events = events;
	event.data.ptr = generic;

	return epoll_ctl(efd, EPOLL_CTL_MOD, generic->fd, &event);
}
