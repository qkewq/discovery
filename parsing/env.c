#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "env.h"

typedef enum env_error_t{
	EENV_SUCCESS = 0,
	EENV_BADMEMORY = -1,
	EENV_INVLDCHAR = -2,
	EENV_NOENVFILE = -3,
	EENV_PERMSDENY = -4,
	EENV_UNKNOWNER = -5,
	EENV_FOVERFLOW = -6,
} env_error_t;

int scan_to(int current, char to, char *file, long file_len){
	for(current; current < file_len; current++){
		if(file[current] == to){
			return current;
		}
	}

	return current;
}

env_error_t get_key_value(int i, char *file, long file_len, key_value_t **new_var){
	int key_start = i;
	int key_len = -1;
	int val_start = -1;
	int val_len = -1;

	key_value_t *var = calloc(1, sizeof(key_value_t));
	if(!var){
		return EENV_BADMEMORY;
	}

	for(i; i < file_len; i++){
		if(file[i] == ' ' || file[i] == '\t' || file[i] == '\n'){
			free(var);
			return EENV_INVLDCHAR;
		}

		if(file[i] == '='){
			key_len = i - key_start;
			break;
		}
	}

	i++;
	val_start = i;

	for(i; i < file_len; i++){
		if(file[i] == ' ' || file[i] == '\t'){
			free(var);
			return EENV_INVLDCHAR;
		}

		if(file[i] == '\n'){
			val_len = i - val_start;
			break;
		}
	}

	if(key_len == -1 || val_start == -1 || val_len == -1){
		free(var);
		return EENV_INVLDCHAR;
	}

	var->key = calloc(key_len + 1, sizeof(char));
	var->value = calloc(val_len + 1, sizeof(char));
	if(!var->key || !var->value){
		free(var->key);
		free(var->value);
		free(var);
		return EENV_BADMEMORY;
	}

	memcpy(var->key, &file[key_start], key_len);
	memcpy(var->value, &file[val_start], val_len);

	*new_var = var;
	return i;
}

env_error_t parse_env(char *file, long file_len, env_t **result){
	env_t *env = calloc(1, sizeof(env_t));
	if(!env){
		return EENV_BADMEMORY;
	}

	key_value_t *current_kv = NULL;

	size_t current_line = 1;
	for(int i = 0; i < file_len; i++){
		// skip whitespace, comments, empty lines
		if(file[i] == ' ' || file[i] == '\t'){
			continue;
		}
		if(file[i] == '#' || file[i] == ';'){
			i = scan_to(i, '\n', file, file_len) - 1;
			continue;
		}
		if(file[i] == '\n'){
			current_line++;
			continue;
		}

		key_value_t *new_var = NULL;
		int res = get_key_value(i, file, file_len, &new_var);
		if(res < 0){
			env_free(env);
			return res;
		}
		new_var->line_num = current_line;
		if(!current_kv){
			env->vars = new_var;
			current_kv = new_var;
		}
		else{
			current_kv->next = new_var;
			current_kv = new_var;
		}
		env->num_vars++;
		i = res - 1;
	}

	*result = env;
	return EENV_SUCCESS;
}

env_error_t errno_to_env_error(int error){
	switch(error){
		case ENOMEM:
			return EENV_BADMEMORY;
		case ENOENT:
			return EENV_NOENVFILE;
		case EACCES:
		case EPERM:
			return EENV_PERMSDENY;
		case EOVERFLOW:
			return EENV_FOVERFLOW;
		default:
			return EENV_UNKNOWNER;
	}
}

int env_init(char *path, env_t **result){
	FILE *file = fopen(path, "r");
	if(!file){
		return errno_to_env_error(errno);
	}

	if(fseek(file, 0, SEEK_END) == -1){
		fclose(file);
		return errno_to_env_error(errno);
	}

	long file_len = ftell(file);
	if(file_len == -1){
		fclose(file);
		return errno_to_env_error(errno);
	}

	char *env_file = calloc(file_len, sizeof(char));
	if(!env_file){
		fclose(file);
		return EENV_BADMEMORY;
	}

	rewind(file);
	if(fread(env_file, sizeof(char), file_len, file) != file_len){
		free(env_file);
		fclose(file);
		return errno_to_env_error(errno);
	}

	fclose(file);

	env_t *env = NULL;
	env_error_t parse_error = parse_env(env_file, file_len, &env);
	if(parse_error != EENV_SUCCESS){
		free(env_file);
		return parse_error;
	}

	free(env_file);
	*result = env;
	return EENV_SUCCESS;
}

int get_num_vars(env_t *env){
	return env->num_vars;
}

char *get_env(env_t *env, char *key){
	key_value_t *current = env->vars;
	while(current){
		if(strcmp(key, current->key) != 0){
			current = current->next;
			continue;
		}
		return current->value;
	}

	return NULL;
}

void env_free(env_t *env){
	key_value_t *current = env->vars;
	key_value_t *next = NULL;
	while(current){
		free(current->key);
		free(current->value);
		next = current->next;
		free(current);
		current = next;
	}

	free(env);
}

const char *env_strerror(int error){
	switch(error){
		case EENV_SUCCESS:
			return "Success";
		case EENV_BADMEMORY:
			return "Memory allocation failed";
		case EENV_INVLDCHAR:
			return "Could not parse env file";
		case EENV_FOVERFLOW:
			return "File size caused overflow";
		case EENV_PERMSDENY:
			return "Access to file was denied";
		case EENV_NOENVFILE:
			return "File could not be found";
		default:
			return "Unknown Error";
	}
}
