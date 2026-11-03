#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"

typedef enum ENV_ERRORS{
	EENV_BADMEMORY = -1,
	EENV_INVLDCHAR = -2,
} ENV_ERRORS;

int scan_to(int current, char to, char *file, long file_len){
	for(current; current < file_len; current++){
		if(file[current] == to){
			return current;
		}
	}

	return current;
}

int get_key_value(int i, char *file, long file_len, key_value_t **new_var){
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

env_t *parse_env(char *file, long file_len){
	env_t *env = calloc(1, sizeof(env_t));
	if(!env){
		return NULL;
	}

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

		//at key
		key_value_t *new_var = NULL;
		int res = get_key_value(i, file, file_len, &new_var);
		if(res < 0){
			env_free(env);
			return NULL;
		}
		new_var->line_num = current_line;
		new_var->next = env->vars;
		env->vars = new_var;
		env->num_vars++;
		i = res - 1;
	}

	return env;
}

env_t *env_init(char *path){
	FILE *file = fopen(path, "r");
	if(!file){
		return NULL;
	}

	if(fseek(file, 0, SEEK_END) == -1){
		fclose(file);
		return NULL;
	}

	long file_len = ftell(file);
	if(file_len == -1){
		fclose(file);
		return NULL;
	}

	char *env_file = calloc(file_len, sizeof(char));
	if(!env_file){
		fclose(file);
		return NULL;
	}

	rewind(file);
	if(fread(env_file, sizeof(char), file_len, file) != file_len){
		free(env_file);
		fclose(file);
		return NULL;
	}

	fclose(file);

	env_t *env = parse_env(env_file, file_len);
	if(!env){
		free(env_file);
		return NULL;
	}

	free(env_file);
	return env;
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
