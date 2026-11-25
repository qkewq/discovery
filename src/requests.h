#ifndef REQUESTS_H
#define REGUESTS_H

#include <curl/curl.h>

typedef enum method_t{
	UNKNOWN_METHOD = 0,
	GET_METHOD,
	PUT_METHOD,
	POST_METHOD,
	PATCH_METHOD,
	DELETE_METHOD,
} method_t;

typedef struct connection_t{
	CURL *conn;
	struct curl_slist *slist;
} connection_t;

typedef struct conn_configs_t{
	char *api_key;
	char *token_id;
	char *pve_url;
} conn_configs_t;

typedef struct request_t{
	connection_t conn;
	method_t method;
	char path[255];
} request_t;

typedef struct response_t{
	long code;
	char *data;
	size_t data_size;
} response_t;

connection_t *pve_conn_init(conn_configs_t *configs);
response_t *make_request(request_t *req, char *base_url);
void connection_cleanup(connection_t *conn);
void free_response(response_t *res);

#endif
