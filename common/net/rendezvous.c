#include "rendezvous_private.h"
#include "std_comm.h"
#include "err.h"
#include "hash.h"
#include "print.h"

/*
 * The implemention of this module is ugly and there is many limitation in using it.
 * We want to modify it later, at present we just use it. (8.3)
 */

struct rendezvous {
	int  count;
	int  total;
	char *name;
	pthread_cond_t  cond;
	pthread_mutex_t mutex;
};

rendezvous_t
rendezvous_create(
	int total_link,
	char *name
)
{
	int len = 0;
	struct rendezvous *rend = NULL;

	rend = (struct rendezvous *)calloc(1, sizeof(*rend));
	if (!rend) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return RENDEVOUS_ERR;
	}
	rend->total = total_link;
	if(name) {
		len = strlen(name);
		rend->name = (char*)calloc(1, len + 1);
		memcpy(rend->name, name, len);
	}
	pthread_cond_init(&rend->cond, NULL);
	pthread_mutex_init(&rend->mutex, NULL);

	return (rendezvous_t)rend;
}

void
rendezvous_destroy(
	rendezvous_t handle
)
{
	struct rendezvous *rend = NULL;	

	if (!handle)
		return;

	rend = (struct rendezvous*)handle;
	if (rend->name)
		FREE(rend);
	
	FREE(rend);
}

int
rendezvous_set(
	rendezvous_t handle
)
{
	struct rendezvous *rend = NULL;

	if (!handle) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	rend = (struct rendezvous *)handle;
	pthread_mutex_lock(&rend->mutex);
	rend->count++;
	if (rend->count >= rend->total) {
		pthread_cond_broadcast(&rend->cond);
		rend->count = 0;
		goto out;
	}
	pthread_cond_wait(&rend->cond, &rend->mutex);

out:
	pthread_mutex_unlock(&rend->mutex);

	return OK;
}
