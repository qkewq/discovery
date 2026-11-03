#ifndef ENV_H
#define ENV_H

typedef struct key_value_t{
	struct key_value_t *next;
	char *key;
	char *value;
	size_t line_num;
	int error;
	int stolen;
} key_value_t;

typedef struct env_t{
	struct key_value_t *vars;
	size_t num_vars;
} env_t;

int env_init(char *path, env_t **env);
int get_num_vars(env_t *env);
char *get_env(env_t *env, char *key);
void env_free(env_t *env);
const char *env_strerror(int error);
char *env_steal_mem(env_t *env, char *key);

#endif
