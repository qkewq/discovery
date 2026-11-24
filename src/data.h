#ifndef DATA_H
#define DATA_H

typedef char mac_addr_t[8];

typedef struct resource_t{
	struct resource_t *next;
	int vmid;
	char *pool;
	char *node;
} resource_t;

typedef struct vm_config_t{
	int vmid;
	mac_addr_t mac;
	size_t config_size;
	char *config_json;
	int seen;
} vm_config_t;

typedef struct vm_configs_list_t{
	pthread_mutex_t lock;
	size_t num_configs;
	map_t *vmid_map;
	map_t *mac_map;
} vm_configs_list_t;

vm_configs_list_t *init_configs_list();
int add_config(vm_configs_list_t *list, vm_config_t *config);
vm_config_t *get_config_by_vmid(vm_configs_list_t *list, int vmid);
vm_config_t *get_config_by_mac(vm_configs_list_t *list, mac_addr_t mac);
void delete_config(vm_configs_list_t *list, int vmid);
void free_vm_configs_list(vm_configs_list_t *list);

#endif
