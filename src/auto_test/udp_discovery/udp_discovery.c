#include "udp_discovery/udp_discovery.h"
#include "udp_discovery/udp_create.h"
#include "ud_err.h"
#include "ud_config_private.h"
#include "std_comm.h"
#include "comm.h"
#include "letter_hash.h"

/*用户处理结构*/
struct udp_discovery {
	struct ud_oper oper;
	letter_t letter_hash;
};

struct udp_discovery_handle {
	int (*handle)(cJSON *data, unsigned long user_data);
};

static struct udp_discovery global_ud_data;

static int
ud_test_prepare_data_get(
	int port_map_cnt,
	unsigned long data
)
{ 
	/* 创建连接前准备连接数据 */
	struct ud_create_data *create_data = NULL;
	if (!data)
		return UD_ERR;

	create_data = (struct ud_create_data*)data;
	pthread_mutex_init(&create_data->mutex, NULL);
	INIT_LIST_HEAD(&create_data->head);
	if (global_ud_data.oper.ud_prepare_data)
		return global_ud_data.oper.ud_prepare_data(
																port_map_cnt, 
																(unsigned long)create_data->user_data);

	return OK;
}

static int
ud_send_func(
	char *data,
	char *method,
	struct ud_create_data *create_data
)
{
	int ret = 0;
	int send_len = 0;
	char *fmt = NULL;
	char *send_data = NULL;
	struct sockaddr_in addr;
	struct ud_config *conf = NULL;

	if (data)
		fmt = "{\"Method\":\"%s\", \"data\":%s}";
	else 
		fmt = "{\"Method\":\"%s\", \"data\":null}";
		 

	send_len = strlen(fmt) + strlen(data);
	send_data = (char*)calloc(1, send_len + 1);
	if (!send_data) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return UD_ERR;
	}
	if (data)
		sprintf(send_data, fmt, method, data);
	else
		sprintf(send_data, fmt, method);

	memset(&addr, 0, sizeof(addr));
	conf = ud_config_get();
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = conf->ip;
	addr.sin_port = htons(conf->port);

	ret = udp_data_send_add(send_data,
				strlen(send_data), 
				(struct address*)&addr, 
				(unsigned long)create_data);
	free(send_data);
	return ret;
}

static int
ud_server_func(
	unsigned long user_data
)
{
	int ret = 0;
	char *data = NULL;
	struct ud_create_data *create_data = NULL;
	
	create_data = (struct ud_create_data*)user_data;
	fprintf(stderr, "ud_server_func\n");
	if (global_ud_data.oper.ud_server_data)
		data = global_ud_data.oper.ud_server_data(
							(unsigned long)create_data->user_data);
	else {
		data = (char*)calloc(sizeof(char), 3);
		memcpy(data, "{}", 2);
	}
	
	ret = ud_send_func(data, "Probe", create_data);

	free(data);
	return ret;
}

static int
ud_client_func(
	unsigned long user_data
)
{
	int ret = 0;
	char *data = NULL;
	struct ud_create_data *create_data = NULL;
	
	create_data = (struct ud_create_data*)user_data;
	fprintf(stderr, "ud_client_func\n");
	if (global_ud_data.oper.ud_client_data)
		data = global_ud_data.oper.ud_client_data(
							(unsigned long)create_data->user_data);
	else {
		data = (char*)calloc(sizeof(char), 3);
		memcpy(data, "{}", 2);
	}
	
	ret = ud_send_func(data, "Hello", create_data);

	free(data);
	return ret;
}

static int
ud_test_err_handle(
	int reason,
	unsigned long user_data
)
{
	/* 当出现错误时调用的函数 */
}

static int
ud_test_send_data(
	unsigned long user_data
)
{
	/* 发送数据时调用的函数 */
	return OK;
}

static int
ud_test_recv_data(
	char *ptr,
	int  size,
	struct address* ta,
	unsigned long user_data
)
{
	/* 接收数据时调用该函数 */
	struct ud_recv_data *recv_data = NULL;
	struct ud_create_data *create_data = (struct ud_create_data*)user_data;
	struct sockaddr_in *addr = (struct sockaddr_in*)ta;

	if (size <= 0)
		return UD_OK;

	recv_data = (typeof(recv_data))calloc(1, sizeof(*recv_data));
	if (!recv_data) {
		PRINT("not enough memory\n");
	}

	recv_data->recv_len = size;
	recv_data->recv_data = (char*)calloc(1, size);
	memcpy(recv_data->recv_data, ptr, size);

	pthread_mutex_lock(&create_data->mutex);
	list_add_tail(&recv_data->node, &create_data->head);
	pthread_mutex_unlock(&create_data->mutex);
	return OK;
}

static int
ud_test_handle_data(
	unsigned long user_data
)
{
	/* 处理数据时，调用该函数 */
	int ret = 0;
	cJSON *root = NULL, *obj = NULL;
	struct ud_create_data *create_data = NULL;
	struct ud_recv_data *recv_data = NULL;
	struct udp_discovery_handle *handle = NULL;
	struct letter_param param;

	create_data = (typeof(create_data))user_data;	

	pthread_mutex_lock(&create_data->mutex);
	recv_data = list_entry(create_data->head.next, typeof(*recv_data), node);
	list_del_init(&recv_data->node);
	pthread_mutex_unlock(&create_data->mutex);

	root = cJSON_Parse(recv_data->recv_data);
	if (!root) {
		fprintf(stderr, "get cjson parse error\n");
		ret = ERR;
		goto out;
	}
	obj = cJSON_GetObjectItem(root, "Method");
	if (!obj) {
		fprintf(stderr, "get object item : no method");
		ret = ERR;
		goto out;
	}
	
	memset(&param, 0, sizeof(param));
	param.name = obj->valuestring;
	handle = (typeof(handle))letter_get(&param, global_ud_data.letter_hash);
	if (!handle) {
		fprintf(stderr, "no handle named %s\n", param.name);
		ret = ERR;
		goto out;
	}
	if (handle->handle) {
		obj = cJSON_GetObjectItem(root, "data");
		ret = handle->handle(obj, (unsigned long)create_data->user_data);
	}

out:
	if (recv_data->recv_data)
		free(recv_data->recv_data);
	free(recv_data);
	if (root)
		cJSON_Delete(root);

	return OK;
}

static int
ud_create_handle(
	char *method,
	int (*handle_func)(cJSON *data, unsigned long user_data)
)
{
	struct udp_discovery_handle *handle = NULL;
	handle = (typeof(handle))calloc(1, sizeof(*handle));
	if (!handle) {
		fprintf(stderr, "calloc %d bytes error: %s\n", strerror(errno));
		exit(0);
	}
	handle->handle = handle_func;

	return letter_add(method, (unsigned long)handle, global_ud_data.letter_hash);
}

static int
ud_add_handle_func(
		struct ud_oper *oper 
)
{
	int ret = 0;

	if ((ret=ud_create_handle("Probe", oper->ud_probe_handle)) != OK)
		return ret;
	if ((ret=ud_create_handle("ProbeMatch", oper->ud_probematch_handle)) != OK)
		return ret;
	if ((ret=ud_create_handle("Hello", oper->ud_hello_handle)) != OK)
		return ret;
	if ((ret=ud_create_handle("Update", oper->ud_update_handle)) != OK)
		return ret;
	if ((ret=ud_create_handle("Bye", oper->ud_bye_handle)) != OK)
		return ret;

	return OK;
}

int
ud_start(
	int user_data_size,
	struct ud_oper *oper
)
{
	struct ud_config *uc = ud_config_get();
	struct create_link_oper cl_oper;

	/* 调用ud_link_create方法，为创建连接设置回调函数 */
	memset(&oper, 0, sizeof(oper));
	if (uc->device_type == UD_DEVICE_SERVER)
		cl_oper.connected_func = ud_server_func;
	else
		cl_oper.connected_func = ud_client_func;
	cl_oper.prepare_data_get = ud_test_prepare_data_get;
	cl_oper.handle_data = ud_test_handle_data;
	cl_oper.udp_recv_data = ud_test_recv_data;
	if (ud_add_handle_func(oper) != OK) 
		return ERR;
	
	return link_create_start("udp_discovery", 
			sizeof(struct udp_discovery) + user_data_size, 
			&cl_oper);
}

static int
ud_setup()
{
	/*初始化数据*/
	fprintf(stderr, "dfdfdf\n");
	ud_config_setup();
	
//	ud_link_create_setup();

	return OK;
}

static int
ud_uninit()
{
	/*当程序结束时，处理该模块的一些数据*/
	letter_destroy(global_ud_data.letter_hash);
	return OK;
}

static int
ud_data_send(
	char *method,
	cJSON *data,
	unsigned long user_data
)
{
	int ret = 0;
	int send_len = 0;
	char *tmp = NULL;
	char *send_data = NULL;
	char *fmt = "{\"Method\":\"%s\", \"data\":%s}";
	struct ud_create_data *create_data = NULL;
	struct ud_config *conf = ud_config_get();
	struct sockaddr_in addr;

	create_data = list_entry(user_data, typeof(*create_data), user_data);
	if (data)
		tmp = cJSON_Print(data);
	else {
		tmp = (char *)calloc(1, 3);
		memcpy(tmp, "{}", 2);
	}
	send_len = strlen(fmt + strlen(tmp) + 1);
	send_data = (char*)calloc(1, send_len);
	if (!send_data) {
		fprintf(stderr, "calloc %d bytes error: %s\n", send_len, strerror(errno));
		exit(0);
	}
	sprintf(send_data, fmt, method, tmp);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(conf->port);
	addr.sin_addr.s_addr = conf->ip;
	ret = udp_data_send_add(send_data, send_len, 
				(struct address*)&addr, (unsigned long)create_data);

	FREE(send_data);
	FREE(tmp);

	return ret;
}

int
ud_probematch_send(
	cJSON *data,
	unsigned long user_data
)
{
	return ud_data_send("ProbeMatch", data, user_data);
}

int
ud_probe_send(
	cJSON *data,
	unsigned long user_data
)
{
	return ud_data_send("Probe", data, user_data);
}

int
ud_hello_send(
	cJSON *data,
	unsigned long user_data
)
{
	return ud_data_send("Hello", data, user_data);
}

int
ud_update_handle(
	cJSON *data,
	unsigned long user_data
)
{
	return ud_data_send("Update", data, user_data);
}

int
ud_bye_handle(
	cJSON *data,
	unsigned long user_data
)
{
	return ud_data_send("Bye", data, user_data);
}

static int
ud_letter_hash_destroy(
	unsigned long user_data
)
{
	free((void*)user_data);
	return OK;
}

/*
 * 该模块最开始运行的程序
 */
int
ud_test_init()
{
	int ret = 0;

	PRINT("hello \n");
	global_ud_data.letter_hash = 
		letter_create(NULL, NULL,ud_letter_hash_destroy);
	if (!global_ud_data.letter_hash) {
		fprintf(stderr, "letter create error\n");
		return ERR;
	}
	PRINT("dfdfdf\n");

	ret = init_register(ud_setup);
	if (ret != OK)
		PANIC("err_msg = %s\n", CUR_ERRMSG_GET());

	ret = uninit_register(ud_uninit);
	if (ret != OK) {
		PANIC("unint_register = %s\n", CUR_ERRMSG_GET());
		return ret;
	}

	return OK;
}

/*
 * 声明该模块在main函数启动之前运行
 */
MOD_INIT(ud_test_init);

