#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "data.h"

resource_t *exetract_router(char *resources, size_t size, size_t last_bracket){
	resource_t *new = calloc(1, sizeof(resource_t));
	if(!new){
		return NULL;
	}

	size_t current = last_bracket;
	int to_next_comma = 0;
	while(current != '}'){
		if(to_next_comma && resources[current] != ','){
			current++;
			continue;
		}
		else if(to_next_comma && resources[current] == ','){
			current++;
			to_next_comma = 0;
			continue;
		}

		if(!new->node){
			if(memcmp(&resources[current], "\"node\":\"", 8) == 0){
				current += 8;
				int length = strcspn(&resources[current], "\"");
				new->node = calloc(length + 1, 1);
				if(!new->node){
					free(new->pool);
					free(new);
					return NULL;
				}
				memcpy(new->node, &resources[current], length);
				current += length + 1;
				continue;
			}
		}
		else if(!new->pool){
			if(memcmp(&resources[current], "\"pool\":\"", 8) == 0){
				current += 8;
				int length = strcspn(&resources[current], "\"");
				new->pool = calloc(length + 1, 1);
				if(!new->pool){
					free(new->node);
					free(new);
					return NULL;
				}
				memcpy(new->pool, &resources[current], length);
				current += length + 1;
				continue;
			}
		}
		else if(!new->vmid){
			if(memcmp(&resources[current], "\"vmid\":", 7) == 0){
				current += 7;
				new->vmid = atoi(&resources[current]);
				to_next_comma++;
				continue;
			}
		}
		current++;
		to_next_comma++;
	}

	if(!new->node || !new->pool || !new->vmid){
		free(new->node);
		free(new->pool);
		free(new);
		return NULL;
	}

	return new;
}

resource_t *extract_current_routers(char *resources, size_t size){
	size_t last_bracket = 0;
	resource_t head = {0};

	for(int i = 0; i < size; i++){
		if(resources[i] == '{'){
			last_bracket == i;
			continue;
		}
		if(memcmp(&resources[i], "\"name\":\"router\"", 15) != 0){
			for(i; i < size; i++){
				if(resources[i] == ',' || resources[i] == '}'){
					break;
				}
			}
		}
		else{
			resource_t *new_node = exetract_router(resources, size, last_bracket);
			if(new_node){
				new_node->next = head.next;
				head.next = new_node;
			}
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
		int num = htoi(data[mac_start + i]) << 4;
		j++;
		num = htoi(data[mac_start + i + j]);
		j++;
		mac.mac[i] = num;
	}

	return mac;
}
