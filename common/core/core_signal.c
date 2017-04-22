#include "std_comm.h"
#include "init.h"
#include "err.h"

#include <signal.h>

struct signal_data {
	int signal;
	pthread_mutex_t mutex;
};

static struct signal_data global_signal_data;

void
signal_handle(
	int signo
)
{
	int ret = 0;

	if (init_test() != OK)
		return;

	pthread_mutex_lock(&global_signal_data.mutex);
	ret = global_signal_data.signal;
	global_signal_data.signal = 1;
	pthread_mutex_unlock(&global_signal_data.mutex);
	if (ret != 0)
		return ;

	uninit();
}

static int
signal_uninit()
{
	pthread_mutex_lock(&global_signal_data.mutex);
	global_signal_data.signal = 1;
	pthread_mutex_unlock(&global_signal_data.mutex);
}

int 
signal_init()
{
	signal(SIGINT, signal_handle);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, signal_handle);

	return uninit_register(signal_uninit);
}

MOD_INIT(signal_init);
