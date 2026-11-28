#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>

#include "data.h"
#include "map.h"

#define VMID_MAP_SIZE 100
#define MAC_MAP_SIZE 100

vm_configs_list_t *init_configs_list(){
	vm_configs_list_t *list = calloc(1, sizeof(vm_configs_list_t));
	if(!list){
		return NULL;
	}

	list->num_configs = 0;
	list->vmid_map = map_init(VMID_MAP_SIZE);
	list->mac_map = map_init(MAC_MAP_SIZE);
	pthread_mutex_init(&list->lock, NULL);

	if(!list->vmid_map || !list->mac_map){
		free_map(list->vmid_map);
		free_map(list->mac_map);
		free(list);
		return NULL;
	}

	return list;
}

int add_config(vm_configs_list_t *list, vm_config_t *config){
	pthread_mutex_lock(&list->lock);
	if(map_insert(list->vmid_map, config->vmid, config, sizeof(vm_config_t)) < 0){
		pthread_mutex_unlock(&list->lock);
		return -1;
	}

	uint64_t mac = 0;
	memcpy(&mac, &config->mac, sizeof(mac_addr_t));
	if(map_insert(list->mac_map, mac, config, sizeof(vm_config_t)) < 0){
		map_delete(list->vmid_map, config->vmid);
		pthread_mutex_unlock(&list->lock);
		return -1;
	}
	pthread_mutex_unlock(&list->lock);

	list->num_configs++;
	return 0;
}

vm_config_t *get_config_by_vmid(vm_configs_list_t *list, int vmid){
	// theoretically only the one writer thread does this so no locking??

	size_t value_size = 0;
	vm_config_t *config = (vm_config_t *)map_lookup(list->vmid_map, vmid, &value_size);

	return config;
}

vm_config_t *get_config_by_mac(vm_configs_list_t *list, mac_addr_t mac){
	size_t value_size = 0;
	uint64_t n_mac = 0;
	memcpy(&n_mac, &mac, sizeof(mac_addr_t));
	pthread_mutex_lock(&list->lock);
	vm_config_t *config = (vm_config_t *)map_lookup(list->mac_map, n_mac, &value_size);
	pthread_mutex_unlock(&list->lock);

	return config;
}

void delete_config(vm_configs_list_t *list, int vmid){
	// writer also only does this so no locking on reads??
	size_t value_size = 0;
	vm_config_t *config = (vm_config_t *)map_lookup(list->vmid_map, vmid, &value_size);
	if(!config){
		return;
	}

	free(config->config_json);
	uint64_t mac = 0;
	memcpy(&mac, &config->mac, sizeof(mac_addr_t));
	pthread_mutex_lock(&list->lock);
	map_delete(list->vmid_map, config->vmid);
	map_delete(list->mac_map, mac);
	list->num_configs--;
	pthread_mutex_unlock(&list->lock);

	free(config->config_json);
}

void free_vm_configs_list(vm_configs_list_t *list){
	free_map(list->mac_map);
	free_map(list->vmid_map);
	pthread_mutex_destroy(&list->lock);
}
