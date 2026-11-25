#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
// #include <stdatomic.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <time.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "server.h"
#include "timers.h"
#include "polling.h"
#include "data.h"

#define HEALTH_CHECK_SEC 5
#define HEALTH_CHECK_NSEC 0

#define RECV_BUFF_SIZE 512
#define SEND_BUFF_SIZE 4096
#define IDLE_CLIENT_CHECK_SEC 30
#define IDLE_CLIENT_CHECK_NSEC 0
#define IDLE_TIMEOUT_SEC 20

#define REQUEST_KNOWN_MAX 7
#define MAX_EPOLL_EVENTS 64

#define SERVER_VERSION_ONE 1

typedef enum client_state_t{
	CTSTATE_WAITING_REQUEST = 0,
	CTSTATE_SENDING_RESPONSE,
	CTSTATE_WAITING_CLEANUP,
} client_state_t;

typedef enum response_codes_t{
	RES_SUCCESS = 0,
	RES_NOT_FOUND = 1,
	RES_SERVER_ERROR = 2,
	RES_BAD_VERSION = 3,
} response_codes_t;

typedef struct client_t{
	struct epoll_generic_t generic;
	struct client_t *next;
	struct sockaddr_in sin;
	char recv_buffer[RECV_BUFF_SIZE];
	size_t recv_used;
	char send_buffer[SEND_BUFF_SIZE];
	size_t send_used;
	size_t send_offset;
	time_t last_activity;
	client_state_t state;
} client_t;

int create_listener(struct sockaddr_in *s_in){
	int sfd = socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), 0);
	if(sfd < 0){
		return -1;
	}

	if(bind(sfd, (struct sockaddr *)s_in, sizeof(struct sockaddr_in)) != 0){
		close(sfd);
		return -1;
	}

	if(listen(sfd, SOMAXCONN) != 0){
		close(sfd);
		return -1;
	}

	return sfd;
}

void thread_cleanup(int listen_fd, int efd, client_t *client, timerfd_t timer){
	close(listen_fd);
	close(efd);
	destroy_timer(timer);

	client_t *current = client;
	client_t *next = NULL;

	while(current){
		next = current->next;

		close(current->generic.fd);
		free(current);

		current = next;
	}
}

void destroy_client(int efd, client_t *client){
	epoll_remove_fd(efd, &client->generic);
	close(client->generic.fd);
	free(client);
}

int idle_checks(int efd, client_t **head){
	client_t *current = *head;
	client_t *prev = NULL;
	time_t time_limit = time(NULL) - IDLE_TIMEOUT_SEC;

	int count = 0;
	while(current){
		if(current->last_activity < time_limit || current->state == CTSTATE_WAITING_CLEANUP){
			if(current == *head){
				*head = current->next;
				destroy_client(efd, current);
				current = *head;
				continue;
			}
			else{
				prev->next = current->next;
				destroy_client(efd, current);
				current = prev->next;
				continue;
			}count++;
		}
		prev = current;
		current = current->next;
	}

	return count;
}

client_t *accept_new_client(int efd, int listener){
	client_t *client = calloc(1, sizeof(client_t));
	if(!client){
		return NULL;
	}

	client->generic.type = PT_CLIENT_CONN;
	client->state = CTSTATE_WAITING_REQUEST;
	client->last_activity = time(NULL);
	socklen_t socklen = sizeof(struct sockaddr_in);
	client->generic.fd = accept(listener, (struct sockaddr *)&client->sin, &socklen);
	if(client->generic.fd < 0){
		free(client);
		return NULL;
	}

	fcntl(client->generic.fd, F_SETFL, fcntl(client->generic.fd, F_GETFL, 0) | O_NONBLOCK);

	if(epoll_add_fd(efd, &client->generic, EPOLLIN) < 0){
		close(client->generic.fd);
		free(client);
		return NULL;
	}

	return client;
}

int move_to_send(int efd, client_t *client){
	if(epoll_mod_fd(efd, &client->generic, EPOLLOUT) == 0){
		client->state = CTSTATE_SENDING_RESPONSE;
	}
	else{
		client->state = CTSTATE_WAITING_CLEANUP;
	}
	return 0;
}

int wait_request(int efd, client_t *client, vm_configs_list_t *list){
	int buff_remaining = RECV_BUFF_SIZE - client->recv_used;
	int num_recv = recv(client->generic.fd, &client->recv_buffer[client->recv_used], buff_remaining, 0);

	if(num_recv <= 0){
		client->state = CTSTATE_WAITING_CLEANUP;
		return 0;
	}

	client->recv_used += num_recv;
	if(client->recv_used < REQUEST_KNOWN_MAX){
		client->state = CTSTATE_WAITING_REQUEST;
		return 0;
	}

	if(client->recv_buffer[0] != SERVER_VERSION_ONE){
		client->send_buffer[0] = RES_BAD_VERSION;
		client->send_used++;
		move_to_send(efd, client);
		return 0;
	}

	mac_addr_t mac = {0};
	memcpy(&mac, &client->recv_buffer[1], sizeof(mac_addr_t));
	// mac_addr_t mac = (mac_addr_t)client->recv_buffer[1];
	vm_config_t *config = get_config_by_mac(list, mac);

	if(!config){
		client->send_buffer[0] = RES_NOT_FOUND;
		client->send_used++;
		move_to_send(efd, client);
		return 0;
	}

	if(config->config_size > SEND_BUFF_SIZE){
		client->send_buffer[0] = RES_SERVER_ERROR;
		client->send_used++;
		move_to_send(efd, client);
		return 0;
	}

	client->send_buffer[0] = RES_SUCCESS;
	client->send_buffer[1] = htons(config->config_size);
	memcpy(&client->send_buffer[3], config->config_json, config->config_size);

	client->send_used += config->config_size + 3;
	move_to_send(efd, client);

	return 0;
}

int send_response(client_t *client){
	int num_send = send(client->generic.fd, &client->send_buffer[client->send_offset], client->send_used - client->send_offset, 0);

	if(num_send <= 0 || num_send == client->send_used - client->send_offset){
		client->state = CTSTATE_WAITING_CLEANUP;
		return 0;
	}

	client->send_offset += num_send;
	return 0;
}

int handle_client(int efd, client_t *client, int events, vm_configs_list_t *list){
	switch(client->state){
		case CTSTATE_WAITING_REQUEST:
			wait_request(efd, client, list);
			break;
		case CTSTATE_SENDING_RESPONSE:
			send_response(client);
			break;
		case CTSTATE_WAITING_CLEANUP:
			return 0;
	}
	client->last_activity = time(NULL);

	return 0;
}

void *server_th(void *server_arg){
	server_arg_t *arg = (server_arg_t *)server_arg;

	epoll_generic_t listener = {0};
	listener.type = PT_LISTENER;

	epoll_generic_t idle_timer = {0};
	idle_timer.type = PT_IDLE_CLIENT_CHECK;

	listener.fd = create_listener(&arg->s_in);
	if(listener.fd < 0){
		*arg->status = TH_SOCK_CREATE_ERROR;
		return NULL;
	}

	int efd = epoll_create1(0);
	if(efd < 0){
		close(listener.fd);
		*arg->status = TH_EPOLL_CREATE_ERROR;
		return NULL;
	}

	idle_timer.fd = create_timer(IDLE_CLIENT_CHECK_SEC, IDLE_CLIENT_CHECK_NSEC);
	if(idle_timer.fd < 0){
		close(efd);
		close(listener.fd);
		*arg->status = TH_EXITTED;
		return NULL;
	}

	if(epoll_add_fd(efd, &idle_timer, EPOLLIN) < 0){
		close(efd);
		close(listener.fd);
		destroy_timer(idle_timer.fd);
		*arg->status = TH_EPOLL_ADD_ERROR;
		return NULL;
	}

	if(epoll_add_fd(efd, &listener, EPOLLIN) < 0){
		close(efd);
		close(listener.fd);
		destroy_timer(idle_timer.fd);
		*arg->status = TH_EPOLL_ADD_ERROR;
		return NULL;
	}

	struct epoll_event events[MAX_EPOLL_EVENTS] = {0};
	client_t *clients = NULL;

	while(1){
		int ready_fds = epoll_wait(efd, events, MAX_EPOLL_EVENTS, 1000);
		if(ready_fds == -1){
			thread_cleanup(listener.fd, efd, clients, idle_timer.fd); // listener, epoll, timer, clients
			*arg->status = TH_EPOLL_WAIT_ERROR;
			return NULL;
		}

		if(*arg->status == TH_PARENT_JOIN){
			thread_cleanup(listener.fd, efd, clients, idle_timer.fd);
			return NULL;
		}

		for(int i = 0; i < ready_fds; i++){
			epoll_generic_t *generic = (epoll_generic_t *)events[i].data.ptr;
			switch(generic->type){
				case PT_LISTENER:
					if(events[i].events & (EPOLLERR | EPOLLHUP)){
						thread_cleanup(listener.fd, efd, clients, idle_timer.fd);
						return NULL;
					}
					client_t *new_client = accept_new_client(efd, generic->fd);
					if(!new_client){
						break;
					}
					new_client->next = clients;
					clients = new_client;
					break;
				case PT_CLIENT_CONN:
					client_t *client = (client_t *)generic;
					if(events[i].events & (EPOLLERR | EPOLLHUP) || client->state == CTSTATE_WAITING_CLEANUP){
						client->state = CTSTATE_WAITING_CLEANUP;
						break;
					}
					handle_client(efd, client, events[i].events, arg->list);
					break;
				case PT_IDLE_CLIENT_CHECK:
					idle_checks(efd, &clients);
					reset_timer(generic->fd);
					break;
				default:
					continue;
			}
		}
	}

}

thread_t *create_server(char *listen_addr, uint16_t listen_port, vm_configs_list_t *list){
	printf("Starting server thread with bind address %s:%d", listen_addr, listen_port);

	thread_t *thread = calloc(1, sizeof(thread_t));
	if(!thread){
		return NULL;
	}

	thread->status = TH_NORMAL;

	thread->arg.list = list;
	thread->arg.status = &thread->status;

	if(inet_pton(AF_INET, listen_addr, (void *)&thread->arg.s_in.sin_addr) != 1){
		free(thread);
		return NULL;
	}

	thread->arg.s_in.sin_port = htons(listen_port);
	thread->arg.s_in.sin_family = AF_INET;

	thread->health_check_timer = create_timer(HEALTH_CHECK_SEC, HEALTH_CHECK_NSEC);
	if(thread->health_check_timer < 0){
		free(thread);
		return NULL;
	}

	if(pthread_create(&thread->thread, NULL, &server_th, &thread->arg) != 0){
		destroy_timer(thread->health_check_timer);
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

	if(pthread_join(thread->thread, NULL) != 0){
		thread_free(thread);
		return -1;
	}

	thread_free(thread);
	return 0;
}

bool restart_server(thread_t *thread){
	thread->status = TH_NORMAL;

	// if(reset_timer(thread->health_check_timer) < 0){
	// 	return false;
	// }

	if(pthread_create(&thread->thread, NULL, &server_th, &thread->arg) != 0){
		return false;
	}

	return true;
}

