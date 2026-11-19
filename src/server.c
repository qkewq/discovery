#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>

#include "server.h"
#include "timers.h"

#define HEALTH_CHECK_SEC 5
#define HEALTH_CHECK_NSEC 0

void *server_th(void *server_arg){
	server_arg_t *arg = (server_arg_t *)server_arg;

	// listener and stuff
	// event loop needs to check status for TH_PARENT_JOIN!!
}

thread_t *create_server(char *listen_addr, uint16_t listen_port, pthread_mutex_t *lock){
	printf("Starting server thread with bind address %s:%d", listen_addr, listen_port);

	thread_t *thread = calloc(1, sizeof(thread_t));
	if(!thread){
		return NULL;
	}

	thread->status = TH_NORMAL;

	thread->arg.lock = lock;
	thread->arg.status = &thread->status;

	if(inet_pton(AF_INET, listen_addr, thread->arg.s_in) != 1){
		free(thread);
		return NULL;
	}

	thread->arg.s_in = htons(listen_port);

	thread->health_check_timer = create_timer(HEALTH_CHECK_SEC, HEALTH_CHECK_NSEC);
	if(thread->health_check_timer < 0){
		free(thread);
		return NULL;
	}

	if(pthread_create(&thread->thread, NULL, &server_th, &thread->arg) != 0){
		destroy_timer(thread->health_check_timer)
		free(thread);
		return NULL;
	}

	printf("Server thread started\n");
	return thread;
}

bool server_alive(thread_t *thread){
	int count = reset_timer(thread->health_check_timer);
	if(count < 0){
		return false;
	}

	if(thread->status != TH_NORMAL){
		return false;
	}

	return true;
}

void thread_free(thread_t *thread){ // only call after thread->thread is cleaned up
	destroy_timer(thread->health_check_timer);
	free(thread);
}

int destroy_server_blocking(thread_t *thread){
	thread->status = TH_PARENT_JOIN;

	destroy_timer(thread->health_check_timer);

	if(pthread_join(thread->thread) != 0){
		thread_free(thread);
		return -1;
	}

	thread_free(thread);
	return 0;
}

bool restart_server(thread_t *thread){
	thread->status = TH_NORMAL;

	if(reset_timer(thread->health_check_timer) < 0){
		return false;
	}

	if(pthread_create(&thread->thread, NULL, &server_th, &thread->arg) != 0){
		return false;
	}

	return true;
}


