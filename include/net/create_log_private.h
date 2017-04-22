#ifndef CREATE_LOG_PRIVATE_H
#define CREATE_LOG_PRIVATE_H

typedef void * log_t;

log_t
log_create(	
	char *app_proto
);

void
log_destroy(
	unsigned long user_data
);

void
log_address_print(
	log_t *log_data
);

void
log_output_type_get(
	int  type,
	char **name,
	char **color
);

#endif
