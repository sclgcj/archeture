#ifndef CONFIG_READ_H
#define CONFIG_READ_H

#include "cJSON.h"
#include "config.h"

enum
{
	CONFIG_TOML_ROOT,
	CONFIG_TOML_LIST,
	CONFIG_TOML_TABLE,
	CONFIG_TOML_TABLE_ARRAY,
	CONFIG_TOML_NORMAL,
	CONFIG_TOML_MAX
};

/*
 * protocol configure
 */
enum
{
	PROTO_TCP, 
	PROTO_UDP,
	PROTO_HTTP,
	PROTO_UNIX_TCP,
	PROTO_UNIX_UDP,
	PROTO_MAX
};

/*
 * server or client
 */
enum
{
	DEV_CLIENT,
	DEV_SERVER,
	DEV_HTTP_CLIENT,
	DEV_MAX
};



/*
 * config_read_oper is a set of operations provided for upstream to to some 
 * extra actions defined by upstream. These function will not affect the default
 * actions.
 */
struct config_read_oper {
	/*
	 * config_read_root() - dispose toml root
	 *
	 * We will call it when we face with the root of the toml configure file.
	 */
	void (*config_read_root)();
	/*
	 * config_read_list() - dispose toml list
	 */
	void (*config_read_list)(char *name);
	/*
	 * config_read_table - dispose toml table
	 */
	void (*config_read_table)(char *name);
	/*
	 * config_read_table_array - dispose toml table array
	 */
	void (*config_read_table_array)(char *name);
};

void
config_read_oper_set(
	struct config_read_oper *oper
);

cJSON *
config_read_get(
	char *table_name
);

void
config_read_del(
	char *table_name
);

int
config_read_handle();

#define CR_INT(json, name, data) do{ \
	cJSON *tmp = cJSON_GetObjectItem(json, name); \
	if (tmp && tmp->valuestring)  \
		data = atoi(tmp->valuestring); \
}while(0)

#define CR_STR(json, name, data) do{ \
	cJSON *tmp = cJSON_GetObjectItem(json, name); \
	if (tmp && tmp->valuestring) \
		memcpy(data, tmp->valuestring, strlen(tmp->valuestring)); \
}while(0)

#define CR_SHORT(json, name, data) do{ \
	cJSON *tmp = cJSON_GetObjectItem(json, name); \
	if (tmp && tmp->valuestring) \
		data = (short)atoi(tmp->valuestring); \
}while(0)

#define CR_USHORT(json, name, data) do{ \
	cJSON *tmp = cJSON_GetObjectItem(json, name); \
	if (tmp && tmp->valuestring) \
		data = (unsigned long)atoi(tmp->valuestring); \
}while(0)

#define CR_IP(json, name, data) do{ \
	cJSON *tmp = cJSON_GetObjectItem(json, name); \
	if (tmp && tmp->valuestring) \
		data = inet_addr(tmp->valuestring); \
}while(0)

#define CR_DURATION(json, name, data) do{ \
	cJSON *tmp = cJSON_GetObjectItem(json, name); \
	int day = 0, hour = 0, min = 0, sec = 0; \
	if (tmp && tmp->valuestring) { \
		sscanf(tmp->valuestring, "%d:%d:%d:%d", &day, &hour, &min, &sec);\
		if (hour >= 24 || min >= 60 || sec >= 60) \
			PANIC("Wrong duration :%s\n", tmp); \
		data = day * 3600 * 24 + hour * 3600 + min * 60 + sec; \
	} \
}while(0);

#define CR_DEV(json, name, data) do{ \
	cJSON *tmp = cJSON_GetObjectItem(json, name); \
	if (tmp && tmp->valuestring) { \
		if (!strcmp(tmp->valuestring, "server")) \
			data = DEV_SERVER; \
		else if (!strcmp(tmp->valuestring, "client")) \
			data = DEV_CLIENT; \
	} \
}while(0)

#define CR_SIZE(json, name, data) do{ \
	int size = 0; \
	char *ch = NULL; \
	cJSON *tmp = cJSON_GetObjectItem(json, name); \
	if (tmp && tmp->valuestring) { \
		size = atoi(tmp->valuestring); \
		ch = strchr(tmp->valuestring, 'K'); \
		if (ch)  \
			size *= 1024; \
		else if ((ch = strchr(tmp->valuestring,'M'))) \
			size *= (1024 * 1024); \
		data = size; \
	} \
}while(0)

#endif
