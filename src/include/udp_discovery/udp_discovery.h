#ifndef TC_UDP_DISCOVERY_2017_03_09_23_16_05_H
#define TC_UDP_DISCOVERY_2017_03_09_23_16_05_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comm.h"
#include "err.h"
#include "cmd.h"
#include "init.h"
#include "print.h"
#include "config_read.h"
#include "config_read.h"
#include "create.h"
#include "json.h"

struct ud_oper {
	int   (*ud_prepare_data)(int port_map_cnt, unsigned long user_data);
	int   (*ud_probe_handle)(cJSON *data, unsigned long user_data);
	int   (*ud_probematch_handle)(cJSON *data, unsigned long user_data);
	int   (*ud_hello_handle)(cJSON *data, unsigned long user_data);
	int		(*ud_update_handle)(cJSON *data, unsigned long user_data);
	int   (*ud_bye_handle)(cJSON *data, unsigned long user_data);
	char* (*ud_server_data)(unsigned long user_data);
	char* (*ud_client_data)(unsigned long user_data);
};


int
ud_start(
	int user_data_size,
	struct ud_oper *oper
);

#endif
