#include <stdlib.h>
#include <string.h>

#include "map.h"
#include "types.h"

map_t *map_init(size_t num_buckets){
	map_t *map = calloc(1, sizeof(map_t));
	if(!map){
		return NULL;
	}

	map->map = calloc(num_buckets, sizeof(map_node_t *));
	if(!map->map){
		free(map);
		return NULL;
	}

	map->num_buckets = num_buckets;
	map->num_keys = 0;
	return map;
}

int map_insert(map_t *map, int key, void *value, size_t value_size){
	map_node_t *node = calloc(1, sizeof(map_node_t));
	if(!node){
		return -1;
	}

	void *value_copy = calloc(value_size, sizeof(char));
	if(!value_copy){
		free(node);
		return -1;
	}

	memcpy(value_copy, value, value_size);

	node->key = key;
	node->value = value_copy;
	node->value_size = value_size;

	size_t index = key % map->num_buckets;

	node->next = map->map[index];
	map->map[index] = node;
	map->num_keys++;

	return 0;
}

void *map_lookup(map_t *map, int key, size_t *value_size){
	size_t index = key % map->num_buckets;

	map_node_t *current = map->map[index];
	while(current){
		if(current->key == key){
			*value_size = current->value_size;
			return current->value;
		}

		current = current->next;
	}

	return NULL;
}

void map_delete(map_t *map, int key){
	size_t index = key % map->num_buckets;

	map_node_t *prev = NULL;
	map_node_t *current = map->map[index];

	if(!current){
		return;
	}

	while(current){
		if(current->key == key){
			if(!prev){
				map->map[index] = current->next;
			}
			else{
				prev->next = current->next;
			}
			free(current->value);
			free(current);
			map->num_keys--;
			return;
		}

		prev = current;
		current = current->next;
	}

	return;
}

void free_map(map_t *map){
	map_node_t *current = NULL;
	map_node_t *next = NULL;

	for(int i = 0; i < map->num_buckets; i++){
		current = map->map[i];

		while(current){
			next = current->next;

			free(current->value);
			free(current);

			current = current->next;
		}
	}

	free(map->map);
	free(map);
}
