#ifndef SERVER_H
#define SERVER_H

typedef enum status_codes_t{
	TH_PARENT_JOIN = -1,
	TH_NORMAL = 0,
	TH_EXITTED = 1,
	TH_SOCK_CREATE_ERROR,
	TH_EPOLL_CREATE_ERROR,
	TH_EPOLL_ADD_ERROR,
	TH_EPOLL_WAIT_ERROR,
} status_codes_t;

typedef struct server_arg_t{
	atomic_int *status;
	// pthread_mutex_t *lock;
	vm_comfigs_list_t *list;
	struct sockaddr_in s_in;
} server_arg_t;

typedef struct thread_t{
	pthread_t thread;
	atomic_int status;
	timer_t health_check_timer;
	struct server_arg_t arg;
} thread_t;

thread_t *create_server(char *listen_addr, uint16_t listen_port, vm_configs_list_t *list);
bool server_alive(thread_t *thread);
int destroy_server_blocking(thread_t *thread);
bool restart_server(thread_t *thread);

#endif
