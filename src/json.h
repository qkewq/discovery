#ifndef JSON_C
#define JSON_C

typedef struct resource_t resource_t;
typedef struct mac_addr_t mac_addr_t;

resource_t *extract_current_routers(char *resources, size_t size);
mac_addr_t extract_mac_addr(char *data, size_t data_size);

#endif
