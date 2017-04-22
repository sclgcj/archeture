#ifndef CREATE_LOG_H
#define CREATE_LOG_H  1

#include "log.h"

enum {
	LOG_SYSTEM,
	LOG_INFO,
	LOG_WARN,
	LOG_DEBUG,
	LOG_FATAL,
	LOG_MAX
};

enum {
	LOG_WRITE_DIRECTLY,
	LOG_WRITE_INDIRECTLY
};

/*
 * log_start() - start store the log string
 * @user_data:  the upstream data
 * @file:	the file to write the log message
 *
 * This function tells the log system that we need to write something as log. we provide 
 * two ways to handle logs: first is to write the log directly to the file(if file is null, * we will write to the stderr); second is to store the log message at first, and write 
 * them when calling log_end() fucntion. 
 *
 * Return: 0 if successful, -1 if not and errno will be set
 */
int
log_start(
	unsigned long user_data,
	char *file
);

/*
 * log_write() - write the log data 
 * @user_data:  the upstream data
 * @name:	the log name
 * @fmt:	the write format
 * @...:	the variable paremeters
 *
 * Return: 0 if successful, -1 if not and errno will be set
 */
int
log_write(
	unsigned long user_data,
	int  direct,
	int  type,
	char *fmt,
	...
);

/*
 * LOG_INFO, LOG_DEBUG, LOG_WARN, LOG_FATAL,LOG_SYSTEM are 
 * used to write each message directly to the log file
 */
#define INFO(d, f, args...)	\
		log_write(d, LOG_WRITE_DIRECTLY, LOG_INFO, \
				"[%s->%s:%d] - "f"\n", __FILE__, __FUNCTION__, __LINE__, ##args)
#define DEBUG(d, f, args...) \
		log_write(d, LOG_WRITE_DIRECTLY, LOG_DEBUG, \
				"[%s->%s:%d] - "f"\n", __FILE__, __FUNCTION__, __LINE__, ##args)
#define WARN(d, f, args...) \
		log_write(d, LOG_WRITE_DIRECTLY, LOG_WARN, \
				"[%s->%s:%d] - "f"\n", __FILE__, __FUNCTION__, __LINE__, ##args)
#define FATAL(d, f, args...)	\
		log_write(d, LOG_WRITE_DIRECTLY, LOG_FATAL, \
				"[%s->%s:%d] - "f"\n", __FILE__, __FUNCTION__, __LINE__, ##args)
#define SYSTEM(d, f, args...) \
		log_write(d, LOG_WRITE_DIRECTLY, LOG_SYSTEM, \
				"[%s->%s:%d] - "f"\n", __FILE__, __FUNCTION__, __LINE__, ##args)


/*
 * log_end() - write out currently stored log message
 * @user_data:  the upstream data
 * @id:	the log id, used to distinguish link logs
 *
 * Return: 0 if successful, -1 if not and errno will be set
 */
int
log_end(
	unsigned long user_data
);

#endif
