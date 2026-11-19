#ifndef TIMERS_H
#define TIMERS_H

typedef int timer_t;

timer_t create_timer(int sec, int nsec);
int reset_timer(timer_t timer);
void destroy_timer(timer_t timer);

#endif
