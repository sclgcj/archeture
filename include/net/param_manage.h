#ifndef PARAM_MANAGE_H
#define PARAM_MANAGE_H

struct param {
	char data[0];
};

struct param_manage_list {
	int param_num;
	char **param_name;
};

#endif
