#include "std_comm.h"
#include "log.h"
#include "err.h"
#include "hash.h"
#include "init_private.h"
#include "log.h"
#include "config_read.h"
#include "print.h"
#include <stdarg.h>

#define LOG_HASH_SIZE 26
#define DEFAULT_LOG_SIZE 512

struct log_data {
	hash_handle_t log_hash;
};


struct log_node {
//	char *name;
	char *data;
	char *file;
	int  id;
	int  direct;
	int  data_pos;
	int  data_size;
	pthread_mutex_t mutex;
	struct hlist_node node;
};

//static struct log_data global_log_data;

static int
log_hash(
	struct hlist_node	*hnode,
	unsigned long		user_data
)
{
	int name = 0;
	struct log_node *log_node;

	if (!hnode) 
		name = (int)user_data;
	else if (hnode) {
		log_node = list_entry(hnode, struct log_node, node);
		name = log_node->id;
	}
	
	return (name);
}

static int
log_hash_get(
	struct hlist_node	*hnode,
	unsigned long		user_data
)
{
	struct log_node *log_node = NULL;

	log_node = list_entry(hnode, struct log_node, node);
	if ((int)user_data != log_node->id)
		return ERR;

	return OK;
}

static int
log_hash_destroy(
	struct hlist_node	*hnode
)
{
	struct log_node *log_node = NULL;

	log_node = list_entry(hnode, struct log_node, node);
	FREE(log_node->data);
	FREE(log_node);

	return OK;
}

struct log_node *
log_node_create(
	int id,
	char *file
)
{
	int len = 0;
	struct log_node *log_node = NULL;
	
	log_node = (struct log_node*)calloc(1, sizeof(*log_node));
	if (!log_node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	if (file && file[0]) {
		len = strlen(file);
		log_node->file = (char*)calloc(len + 1, sizeof(char));
		if (!log_node->file) {
			FREE(log_node);
			ERRNO_SET(NOT_ENOUGH_MEMORY);
			return NULL;
		}
		memcpy(log_node->file, file, len);
	}
	log_node->id	= id;
	log_node->data_pos = 0;
	log_node->data_size = DEFAULT_LOG_SIZE;
	pthread_mutex_init(&log_node->mutex, NULL);
	log_node->data = (char*)calloc(log_node->data_size, sizeof(char));
	if (!log_node->data) {
		FREE(log_node->file);
		FREE(log_node);
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}

	return log_node;
}

void
log_data_address(
	log_data_t data
)
{
	struct log_data *ldata = NULL;

	ldata = (struct log_data*)data;
}

int
log_data_start(
	log_data_t data,
	int  id,
	char *file
)
{
	int len = 0;
	struct hlist_node *hnode = NULL;
	struct log_node *log_node = NULL;
	struct log_data *log_data = NULL;

	if (!data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	log_data = (struct log_data *)data;
	hnode = hash_get(
			log_data->log_hash, 
			(unsigned long)id, 
			(unsigned long)id);
	if (hnode) {
		log_node = list_entry(hnode, struct log_node, node);
		log_node->data_pos = 0;
		memset(log_node->data, 0, log_node->data_size);
		return OK;
	}
	else {
		log_node = log_node_create(id, file);
		return hash_add(log_data->log_hash, &log_node->node, id);
	}

}

static int
log_data_string_set(
	struct log_node	*log_node, 
	char			*fmt,
	va_list			arg_ptr
)
{
	int ret = 0;
	int vs_ret = 0;
	va_list save;

	pthread_mutex_lock(&log_node->mutex);
	while (1) {
		va_copy(save, arg_ptr);
		vs_ret += vsnprintf(
				log_node->data + log_node->data_pos , 
				log_node->data_size - log_node->data_pos, 
				fmt, 
				save);
		va_end(save);
		ret = strlen(log_node->data);
		if (ret + 1 < log_node->data_size) 
			break;
		//log_node->data_pos += vs_ret;
		log_node->data_size *= 2;
		log_node->data = (char*)realloc(
						log_node->data, 
						log_node->data_size);
		if (!log_node->data) {
			ERRNO_SET(NOT_ENOUGH_MEMORY);
			return ERR;
		}
		memset(
			log_node->data + log_node->data_pos, 
			0, 
			log_node->data_size - log_node->data_pos);
	}
	pthread_mutex_unlock(&log_node->mutex);

	return OK;
}

static int
log_data_string_write(
	struct log_node *log_node
)
{
	FILE *fp = NULL;

	pthread_mutex_lock(&log_node->mutex);
	if (!log_node->file) 
		fprintf(stderr, "%s", log_node->data);
	else {
		fp = fopen(log_node->file, "w+");
		if (!fp) {
			pthread_mutex_unlock(&log_node->mutex);
			ERRNO_SET(OPEN_FILE_ERR);
			return ERR;
		}
		fwrite(log_node->data, sizeof(char), log_node->data_pos, fp);
		fclose(fp);
	} 
	pthread_mutex_unlock(&log_node->mutex);

	return OK;
}

int
log_data_write(
	log_data_t	data,
	int		id,
	char		*fmt,
	va_list		arg_ptr
)
{
	int ret = 0;
	struct hlist_node  *hnode = NULL;
	struct log_node *log_node;
	struct log_data *log_data;

	if (!data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	log_data = (struct log_data *)data;
	hnode = hash_get(
			log_data->log_hash, 
			(unsigned long)id, 
			(unsigned long)id);
	if (!hnode) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	log_node = list_entry(hnode, struct log_node, node);

	ret = log_data_string_set(log_node, fmt, arg_ptr);
	if (ret != OK)
		return ret;

	log_node->data_pos = strlen(log_node->data);
	//return log_data_string_write(log_node);

	return OK;
}

int
log_data_end(
	log_data_t data,
	int id
)
{
	int ret = 0;
	struct hlist_node *hnode = NULL;
	struct log_node *log_node = NULL;
	struct log_data *log_data = NULL;

	log_data = (struct log_data *)data;
	if (!log_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	hnode = hash_get(
			log_data->log_hash, 
			(unsigned long)id, 
			(unsigned long)id);
	if (!hnode){
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	log_node = list_entry(hnode, struct log_node, node);	

	ret = log_data_string_write(log_node);

	memset(log_node->data, 0, log_node->data_pos);
	log_node->data_pos = 0;

	return ret;
}

int
log_data_destroy(
	log_data_t data
)
{
	struct log_data *log_data = (struct log_data*)data;
	if (!log_data)
		return OK;

	return hash_destroy(log_data->log_hash);
}

static int
log_data_init(
	int total_link,
	struct log_data *data
)
{
	data->log_hash = hash_create(
					total_link,
					log_hash, 
					log_hash_get, 
					log_hash_destroy);
	if (data->log_hash == HASH_ERR)
		PANIC("create log hash error\n");

	return OK;
	//return uninit_register(log_uninit);
}

log_data_t
log_data_create(
	int total_link	
)
{
	struct log_data *log_data = NULL;

	log_data = (struct log_data *)calloc(1, sizeof(struct log_data));
	if (!log_data) {
		PANIC("not enough memroy for %d bytes: %s\n", 
				sizeof(struct log_data));
	}
	
	log_data_init(total_link, log_data);
	return (log_data_t)log_data;
}

