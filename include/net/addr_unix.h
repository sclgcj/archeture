#ifndef ADDR_UNIX_H
#define ADDR_UNIX_H 1

#include "std_comm.h"
#define UNIX_PATH_MAX 108
struct addr_unix {
	char unix_path[UNIX_PATH_MAX];
};

#endif
