#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/epoll.h>
#include <stdbool.h>
#include <unistd.h>

#include "env.h"
#include "requests.h"
#include "map.h"
#include "timers.h"
#include "data.h"
#include "server.h"
#include "json.h"
#include "polling.h"

#define DEFAULT_ENV ".env"
#define PERSIST_FILE "/opt/discovery/persist.json"
#define DEFAULT_LISTEN_PORT 8080
#define DEFAULT_LISTEN_ADDR "127.0.0.1"
#define DEFAULT_POLL_TIME_SEC 5
#define VMID_MAP_SIZE 100
#define HEALTH_CHECK_RETRYS 10

typedef struct env_configs_t{
	size_t poll_timer_sec;
	char *listen_addr;
	uint16_t listen_port;
	conn_configs_t conn_configs;
} env_configs_t;

bool health_check(thread_t *thread){
	if(server_alive(thread)){
		return true;
	}

	for(int i = 0; i < HEALTH_CHECK_RETRYS; i++){
		if(restart_server(thread)){
			return true;
		}
		sleep(1);
	}

	return false;
}

void free_and_reset_seen(vm_configs_list_t *list){
	for(int i = 0; i < list->vmid_map->num_buckets; i++){
		map_node_t *node = list->vmid_map->map[i];
		while(node){
			vm_config_t *config = (vm_config_t *)node->value;
			if(!config->seen){
				int vmid = config->vmid;
				node = node->next;
				delete_config(list, vmid);
			}
			else{
				config->seen = 0;
				node = node->next;
			}
		}
	}
}

int update_list(vm_configs_list_t *list, connection_t *conn, env_configs_t *configs){
	request_t req = {
		.conn = *conn,
		.method = GET_METHOD,
		.path = "/cluster/resources?type=vm",
	};
	response_t *res = make_request(&req, configs->conn_configs.pve_url);
	if(!res){
		return -1;
	}

	if(res->code != 200){
		free_response(res);
		return -1;
	}

	resource_t *resources = extract_current_routers(res->data, res->data_size);
	free_response(res);
	res = NULL;

	resource_t dummy = {0};
	dummy.next = resources;
	resource_t *current = resources;
	resource_t *next = NULL;
	resource_t *prev = &dummy;

	while(current){
		next = current->next;
		vm_config_t *config = get_config_by_vmid(list, current->vmid);
		if(config){
			config->seen = 1;
			prev->next = next;
			free(current->node);
			free(current->pool);
			free(current);
			current = next;
			continue;
		}
		prev = current;
		current = next;
	}

	free_and_reset_seen(list);

	prev = &dummy;
	current = prev->next;
	next = NULL;

	while(current){
		req.conn = *conn;
		req.method = GET_METHOD;
		if(snprintf(req.path, sizeof(req.path), "/nodes/%s/qemu/%d/config", current->node, current->vmid) > sizeof(req.path)){
			current = current->next;
			continue;
		}

		res = make_request(&req, configs->conn_configs.pve_url);
		if(!res){
			current = current->next;
			continue;
		}

		if(res->code != 200){
			free_response(res);
			current = current->next;
			continue;
		}

		vm_config_t *new_config = calloc(1, sizeof(vm_config_t));
		if(!new_config){
			free_response(res);
			current = current->next;
			continue;
		}

		mac_addr_t zero_mac = {0};
		new_config->vmid = current->vmid;
		new_config->mac = extract_mac_addr(res->data, res->data_size);
		if(memcmp(&new_config->mac, &zero_mac, sizeof(mac_addr_t)) == 0){
			free_response(res);
			free(new_config);
			current = current->next;
			continue;
		}
		int pool_len = strlen(current->pool);
		char *new_json = calloc(10 + pool_len + res->data_size, 1);
		if(!new_json){
			free(new_config);
			free_response(res);
			current = current->next;
			continue;
		}

		// ts so scuffed :(
		memcpy(new_json, res->data, 9); // magic
		memcpy(&new_json[9], "\"pool\":\"", 8); // magic
		memcpy(&new_json[17], current->pool, pool_len);
		memcpy(&new_json[17 + pool_len], "\",", 2);
		memcpy(&new_json[19 + pool_len], &res->data[9], res->data_size - 9);

		new_config->config_json = new_json;
		new_config->config_size = res->data_size + pool_len + 10;

		if(add_config(list, new_config) == -1){
			free(new_json);
			free_response(res);
			free(new_config);
			current = current->next;
			continue;
		}

		current = current->next;
	}

	current = dummy.next;
	next = NULL;
	while(current){
		next = current->next;
		free(current->node);
		free(current->pool);
		free(current);
		current = next;
	}

	return 0;
}

int discovery(env_configs_t *configs, vm_configs_list_t *list){
	timerfd_t poll_timer = create_timer(configs->poll_timer_sec, 0);
	if(!poll_timer){
		return -1;
	}

	int efd = epoll_create1(0);
	if(efd < 0){
		destroy_timer(poll_timer);
		return -1;
	}

	connection_t *conn = pve_conn_init(&configs->conn_configs);
	if(!conn){
		close(efd);
		destroy_timer(poll_timer);
		return -1;
	}

	thread_t *thread = create_server(configs->listen_addr, configs->listen_port, list);
	if(!thread){
		connection_cleanup(conn);
		close(efd);
		destroy_timer(poll_timer);
		return -1;
	}

	epoll_generic_t timers[2] = {0};

	timers[0].fd = poll_timer;
	timers[0].type = PT_POLL_UPSTREAM;

	timers[1].fd = thread->health_check_timer;
	timers[1].type = PT_THREAD_TIMER;

	if(epoll_add_fd(efd, &timers[0], EPOLLIN) < 0){
		connection_cleanup(conn);
		close(efd);
		destroy_timer(poll_timer);
		destroy_server_blocking(thread);
		return -1;
	}
	if(epoll_add_fd(efd, &timers[1], EPOLLIN) < 0){
		connection_cleanup(conn);
		close(efd);
		destroy_timer(poll_timer);
		destroy_server_blocking(thread);
		return -1;
	}

	struct epoll_event events[2] = {0};
	while(1){
		int ready_timers = epoll_wait(efd, &events[0], sizeof(events) / sizeof(struct epoll_event), -1);
		if(ready_timers == -1){
			connection_cleanup(conn);
			close(efd);
			destroy_timer(poll_timer);
			destroy_server_blocking(thread);
			return -1;
		}

		for(int i = 0; i < ready_timers; i++){
			if(events[i].events & (EPOLLERR | EPOLLHUP)){
				connection_cleanup(conn);
				close(efd);
				destroy_timer(poll_timer);
				destroy_server_blocking(thread);
				return -1;
			}
			epoll_generic_t *generic = (epoll_generic_t *)events[i].data.ptr;
			switch(generic->type){
				case PT_POLL_UPSTREAM:
					update_list(list, conn, configs);
					reset_timer(generic->fd);
					break;
				case PT_THREAD_TIMER:
					if(!health_check(thread)){
						connection_cleanup(conn);
						close(efd);
						destroy_timer(poll_timer);
						destroy_server_blocking(thread);
						return -1;
					}
					reset_timer(generic->fd);
					break;
			}
		}
	}

	return 0;
}

void free_configs(env_configs_t *configs){
	free(configs->conn_configs.api_key);
	free(configs->conn_configs.token_id);
	free(configs->conn_configs.pve_url);
	free(configs->listen_addr);
}

void missing_key(char *key){
	printf("Required key missing: %s\n", key);
}

int populate_map(vm_configs_list_t *list){
	// FILE *file = fopen(PERSIST_FILE,"r");
	// if(!file){
	// 	return -1;
	// }

	// if(fseek(file, 0, SEEK_END) == -1){
	// 	fclose(file);
	// 	return -1;
	// }

	// long file_len = ftell(file);
	// if(file_len == -1){
	// 	fclose(file);
	// 	return -1;
	// }

	// int num_present = file_len / sizeof(router_t);

	// router_t *data = calloc(num_present, sizeof(router_t));
	// if(!data){
	// 	fclose(file);
	// 	return -1;
	// }

	// rewind(file);
	// if(fread(file, sizeof(router_t), num_present, file) != file_len){
	// 	free(data);
	// 	fclose(file);
	// 	return -1;
	// }

	// fclose(file);

	// for(int i = 0; i < num_present; i++){
	// 	if(vmid_map_insert(map, data[i].vmid, &data[i]) < 0){
	// 		free(data);
	// 		return -1;
	// 	}
	// }

	// free(data);
	return 0;
}

int main(int argc, char **argv){
	char *env_path = DEFAULT_ENV;
	if(argc > 1){
		env_path = &argv[1][0];
	}

	env_configs_t *configs = calloc(1, sizeof(env_configs_t));
	if(!configs){
		return 1;
	}

	env_t *env = NULL;
	int env_error = env_init(env_path, &env);
	if(env_error != 0){
		printf("Error getting env file: %s\n", env_strerror(env_error));
		free(configs);
		return 1;
	}

	configs->listen_addr = calloc(strlen(DEFAULT_LISTEN_ADDR) + 1, 1);
	if(!configs->listen_addr){
		env_free(env);
		free(configs);
		return 1;
	}
	memcpy(configs->listen_addr, DEFAULT_LISTEN_ADDR, strlen(DEFAULT_LISTEN_ADDR));
	configs->listen_port = DEFAULT_LISTEN_PORT;
	configs->poll_timer_sec = DEFAULT_POLL_TIME_SEC;

	char *value = NULL;
	value = get_env(env, "LISTEN_ADDR");
	if(value){
		free(configs->listen_addr);
		configs->listen_addr = env_steal_value_mem(env, "LISTEN_ADDR");
		value = NULL;
	}
	value = get_env(env, "LISTEN_PORT");
	if(value){
		configs->listen_port = atoi(value);
		value = NULL;
	}
	value = get_env(env, "POLL_TIME_SECONDS");
	if(value){
		configs->poll_timer_sec = atoi(value);
		value = NULL;
	}

	int missing_configs = 0;
	configs->conn_configs.pve_url = env_steal_value_mem(env, "PROXMOX_URL");
	if(!configs->conn_configs.pve_url){
		missing_key("PROXMOX_URL");
		missing_configs++;
	}
	configs->conn_configs.api_key = env_steal_value_mem(env, "PROXMOX_API_KEY");
	if(!configs->conn_configs.api_key){
		missing_key("PROXMOX_API_KEY");
		missing_configs++;
	}
	configs->conn_configs.token_id = env_steal_value_mem(env, "PROXMOX_TOKEN_ID");
	if(!configs->conn_configs.token_id){
		missing_key("PROXMOX_TOKEN_ID");
		missing_configs++;
	}

	env_free(env);

	if(missing_configs){
		printf("Missing %d requried key(s) in config\n", missing_configs);
		free_configs(configs);
		return 1;
	}

	vm_configs_list_t * list = init_configs_list();
	if(!list){
		free_configs(configs);
		return 1;
	}
	if(populate_map(list) < 0){
		free_configs(configs);
		free_vm_configs_list(list);
		return 1;
	}

	discovery(configs, list);
	free_configs(configs);
	free_vm_configs_list(list);

	return 0;
}
