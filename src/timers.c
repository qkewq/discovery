#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/timerfd.h>

#include "timers.h"

timerfd_t create_timer(int sec, int nsec){
	timerfd_t timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if(timer < 0){
		return -1;
	}

	struct itimerspec spec = {0};
	spec.it_value.tv_sec = sec;
	spec.it_value.tv_nsec = nsec;
	spec.it_interval.tv_sec = sec;
	spec.it_interval.tv_nsec = nsec;

	if(timerfd_settime(timer, 0, &spec, NULL) < 0){
		close(timer);
		return -1;
	}

	return timer;
}

int reset_timer(timerfd_t timer){
	uint64_t count = 0;
	if(read(timer, &count, sizeof(uint64_t)) != sizeof(uint64_t)){
		return -1;
	}

	return count;
}

void destroy_timer(timerfd_t timer){
	close(timer);
}
