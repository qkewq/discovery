#ifndef TIMERS_H
#define TIMERS_H

typedef int timerfd_t;

timerfd_t create_timer(int sec, int nsec);
int reset_timer(timerfd_t timer);
void destroy_timer(timerfd_t timer);

#endif
