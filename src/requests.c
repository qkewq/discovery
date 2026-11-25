#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// #include <curl/curl.h>

#include "requests.h"

#define AUTH_HEADER "Authorization: PVEAPIToken=%s=%s"
#define FULL_URL_MAX 512

size_t response_callback(char *data, size_t size, size_t nmemb, void *userdata){
	size_t realsize = nmemb;
	response_t *res = (response_t *)userdata;

	char *ptr = realloc(res->data, res->data_size + realsize + 1);
	if(!ptr){
		return 0;
	}

	res->data = ptr;
	memcpy(&(res->data[res->data_size]), data, realsize);
	res->data_size += realsize;
	res->data[res->data_size] = 0;

	return realsize;
}

connection_t *pve_conn_init(conn_configs_t *configs){
	connection_t *conn = calloc(1, sizeof(connection_t));
	if(!conn){
		return NULL;
	}

	if(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK){
		connection_cleanup(conn);
		return NULL;
	}

	conn->conn = curl_easy_init();
	if(!conn->conn){
		connection_cleanup(conn);
		return NULL;
	}

	struct curl_slist *temp_slist = NULL;

	int auth_len = 0;
	auth_len += strlen(configs->token_id);
	auth_len += strlen(configs->api_key);
	auth_len += strlen(AUTH_HEADER) + 1;
	char *auth_header = calloc(auth_len, sizeof(char));
	if(!auth_header){
		connection_cleanup(conn);
		return NULL;
	}
	if(snprintf(auth_header, auth_len, AUTH_HEADER, configs->token_id, configs->api_key) < 0){
		free(auth_header);
		connection_cleanup(conn);
		return NULL;
	}
	temp_slist = curl_slist_append(NULL, auth_header);
	if(!temp_slist){
		free(auth_header);
		connection_cleanup(conn);
		return NULL;
	}
	conn->slist = temp_slist;
	free(auth_header);

	if(curl_easy_setopt(conn->conn, CURLOPT_HTTPHEADER, conn->slist) != CURLE_OK){
		connection_cleanup(conn);
		return NULL;
	}

	// if(curl_easy_setopt(conn->conn, CURLOPT_URL, configs->pve_url) != CURLE_OK){
	// 	connection_cleanup(conn);
	// 	return NULL;
	// }

	if(curl_easy_setopt(conn->conn, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK){
		connection_cleanup(conn);
		return NULL;
	}

	if(curl_easy_setopt(conn->conn, CURLOPT_WRITEFUNCTION, response_callback) != CURLE_OK){
		connection_cleanup(conn);
		return NULL;
	}

	// curl_easy_setopt(conn->conn, CURLOPT_SSL_VERIFYPEER, 0L);
	// curl_easy_setopt(conn->conn, CURLOPT_SSL_VERIFYHOST, 0L);

	return conn;
}

response_t *make_request(request_t *req, char *base_url){
	printf("Making request to endpoint: %s\n", req->path);
	response_t *res = calloc(1, sizeof(response_t));
	if(!res){
		return NULL;
	}

	if(req->method != GET_METHOD){
		//set opts but no no for now
		free(res);
		return NULL;
	}

	if(curl_easy_setopt(req->conn.conn, CURLOPT_WRITEDATA, (void *)res) != CURLE_OK){
		free(res);
		return NULL;
	}

	char full_url[FULL_URL_MAX] = {0};
	if(snprintf(full_url, FULL_URL_MAX, "%s%s", base_url, req->path) < 0){
		free(res);
		return NULL;
	}
	if(curl_easy_setopt(req->conn.conn, CURLOPT_URL, full_url) != CURLE_OK){
		free(res);
		return NULL;
	}

	if(curl_easy_perform(req->conn.conn) != CURLE_OK){
		free(res);
		return NULL;
	}

	if(curl_easy_getinfo(req->conn.conn, CURLINFO_RESPONSE_CODE, &res->code) != CURLE_OK){
		free(res->data);
		free(res);
		return NULL;
	}
	printf("Request completed with code: %d\n", res->code);

	return res;
}

void connection_cleanup(connection_t *conn){
	curl_slist_free_all(conn->slist);
	curl_easy_cleanup(conn->conn);
	curl_global_cleanup();
	free(conn);
}

void free_response(response_t *res){
	free(res->data);
	free(res);
}
