#ifndef GLOBAL_LOG_PRIVATE_H
#define GLOBAL_LOG_PRIVATE_H

#include "create_log.h"

int
global_log_write(
	int type,
	char *fmt,
	...
);

#define GINFO(f, args...) \
		global_log_write(LOG_INFO, "[%s->%s:%d] - f\n", \
				__FILE__, __FUNCTION__, __LINE__, ##args)

#define GDEBUG(f, args...) \
		global_log_write(LOG_DEBUG, "[%s->%s:%d] - f\n", \
				__FILE__, __FUNCTION__, __LINE__, ##args)

#define GWARN(f, args...) \
		global_log_write(LOG_WARN, "[%s->%s:%d] - f\n", \
				__FILE__, __FUNCTION__, __LINE__, ##args)

#define GFATAL(f, args...) \
		global_log_write(LOG_FATAL, "[%s->%s:%d] - f\n", \
				__FILE__, __FUNCTION__, __LINE__, ##args)

#define GSYSTEM(f, args...) \
		global_log_write(LOG_SYSTEM, "[%s->%s:%d] - f\n", \
				__FILE__, __FUNCTION__, __LINE__, ##args)

#endif
