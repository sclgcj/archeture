#ifndef RENDEZVOUS_H
#define RENDEZVOUS_H

typedef void* rendezvous_t;
#define RENDEVOUS_ERR (void*)-1

rendezvous_t
rendezvous_create(
	int total_link,
	char *name
);

void
rendezvous_destroy(
	rendezvous_t handle
);

int
rendezvous_set(
	rendezvous_t handle
);



#endif
