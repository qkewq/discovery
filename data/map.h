#ifndef MAP_H
#define MAP_H

typedef struct map_node_t{
	struct map_node_t *next;
	int key;
	size_t value_size;
	void *value;
} map_node_t;

typedef struct map_t{
	size_t num_buckets;
	size_t num_keys;
	struct map_node_t **map;
} map_t;

map_t *map_init(size_t num_buckets);
int map_insert(map_t *map, int key, void *value, size_t value_size);
void *map_lookup(map_t *map, int key, size_t *value_size);
void map_delete(map_t *map, int key);
void free_map(map_t *map);

#endif
