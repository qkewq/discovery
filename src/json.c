#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "json.h"
#include "data.h"

resource_t *extract_router(char *resource){
	resource_t *new = calloc(1, sizeof(resource_t));
	if(!new){
		return NULL;
	}

	char *value = NULL;
	int length = 0;

	value = strstr(resource, "\"vmid\":");
	if(!value){
		free(new);
		return NULL;
	}
	value += 7;
	new->vmid = atoi(value);
	value = NULL;

	value = strstr(resource, "\"node\":\"");
	if(!value){
		free(new);
		return NULL;
	}
	value += 8;
	length = strcspn(value, "\"");
	new->node = calloc(length + 1, 1);
	if(!new->node){
		free(new);
		return NULL;
	}
	memcpy(new->node, value, length);
	value = NULL;

	value = strstr(resource, "\"pool\":\"");
	if(!value){
		free(new->node);
		free(new);
		return NULL;
	}
	value += 8;
	length = strcspn(value, "\"");
	new->pool = calloc(length + 1, 1);
	if(!new->pool){
		free(new->node);
		free(new);
		return NULL;
	}
	memcpy(new->pool, value, length);

	return new;
}

bool is_router(char *resource){
	if(strstr(resource, "\"name\":\"router\"")){
		return true;
	}

	return false;
}

resource_t *extract_current_routers(char *resources, size_t size){
	char *open_brace_pos = NULL;
	char *close_brace_pos = NULL;
	resource_t head = {0};
	int i = 0;

	for(i; i < size; i++){
		if(resources[i] == '['){
			break;
		}
	}

	while(1){
		open_brace_pos = (char *)memchr(&resources[i], '{', size - i);
		if(!open_brace_pos){
			return head.next;
		}
		i = open_brace_pos - resources;
		close_brace_pos = (char *)memchr(&resources[i], '}', size - i);
		if(!close_brace_pos){
			return head.next;
		}
		i = close_brace_pos - resources;

		*close_brace_pos = '\0';
		if(!is_router(open_brace_pos)){
			continue;
		}

		resource_t *new_node = extract_router(open_brace_pos + 1);
		if(new_node){
			new_node->next = head.next;
			head.next = new_node;
		}

	}

	return head.next;
}

int htoi(char c){
	switch(c){
		case '0':
			return 0;
		case '1':
			return 1;
		case '2':
			return 2;
		case '3':
			return 3;
		case '4':
			return 4;
		case '5':
			return 5;
		case '6':
			return 6;
		case '7':
			return 7;
		case '8':
			return 8;
		case '9':
			return 9;
		case 'A':
		case 'a':
			return 10;
		case 'B':
		case 'b':
			return 11;
		case 'C':
		case 'c':
			return 12;
		case 'D':
		case 'd':
			return 13;
		case 'E':
		case 'e':
			return 14;
		case 'F':
		case 'f':
			return 15;
	}

	return -1;
}

// scuffed ahh, must change later
mac_addr_t extract_mac_addr(char *data, size_t data_size){
	int mac_start = -1;
	for(int i = 0; i < data_size; i++){
		if(memcmp(&data[i], "\"net0\":", 7) == 0){
			for(i; i < data_size; i++){
				if(data[i] == '='){
					mac_start = i + 1;
					break;
				}
			}
			break;
		}
	}

	mac_addr_t mac = {0};

	if(mac_start < 0){
		return mac;
	}

	int j = 0;
	for(int i = 0; i < 6; i++){
		int num = htoi(data[mac_start + i + j]) << 4;
		j++;
		num += htoi(data[mac_start + i + j]);
		j++;
		mac.mac[i] = num;
	}

	return mac;
}
