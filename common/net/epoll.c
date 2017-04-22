#include "epoll_private.h"
#include "err.h"
#include "hash.h"
#include "init.h"
#include "create.h"
#include "print.h"
#include "thread.h"
#include "heap_timer.h"
#include "global_log_private.h"

struct global_epoll_data {
	int duration;
	int epoll_fd;
	struct epoll_oper oper;
};

#define EPOLL_EVENT_MAX  1024
static struct global_epoll_data global_epoll_data;

static int
epoll_data_ctl(
	int	sock,
	int	cmd,
	int	event,	
	void	*ptr
)
{
	int ret = 0;
	struct epoll_event epoll_event, *tmp = NULL;

	if (ptr)
	{
		memset(&epoll_event, 0, sizeof(epoll_event));
		epoll_event.events = event;
		epoll_event.data.ptr = ptr;
		tmp = &epoll_event;
	}

	ret = epoll_ctl(
			global_epoll_data.epoll_fd, 
			cmd,
			sock, 
			tmp);
	if (ret < 0) {
	//	GINFO("epoll_ctl err: %s\n", strerror(errno));
		ERRNO_SET(CTL_EPOLL_ERR);
		return ERR;
	}

	return OK;
}

int
epoll_data_add(
	int		sock,
	int		event,	
	unsigned long	epoll_data
)
{
	return epoll_data_ctl(sock, EPOLL_CTL_ADD, event, (void*)epoll_data);
}

int
epoll_data_mod(
	int		sock,
	int		event,	
	unsigned long	epoll_data
)
{
	return epoll_data_ctl(sock, EPOLL_CTL_MOD, event, (void*)epoll_data);
}

int
epoll_data_recv_mod(
	int		sock,
	unsigned long	data
)
{
	return epoll_data_ctl(sock, EPOLL_CTL_MOD, EVENT_READ, (void*)data);
}

int
epoll_data_send_mod(
	int		sock,
	unsigned long	data
)
{
	return epoll_data_ctl(sock, EPOLL_CTL_MOD, EVENT_WRITE, (void*)data);
}

int
epoll_data_del(
	int sock
)
{
	return epoll_data_ctl(sock, EPOLL_CTL_DEL, 0, NULL);
}

int
epoll_config_set(
	int duration,
	struct epoll_oper *oper
)
{
	global_epoll_data.duration = duration;
	memcpy(&global_epoll_data.oper, oper, sizeof(*oper));
	return OK;
}

static int
epoll_check_duration()
{
	int tick = 0;	

	tick = heap_timer_tick_get();
	GINFO("tick = %d, duration = %d\n", tick, global_epoll_data.duration);
	if (global_epoll_data.duration == 0)
		return ERR;
	if (tick >= global_epoll_data.duration) {
		thread_exit_wait();
		return OK;
	}
	
	return ERR;
}

int
epoll_start()
{
	int i = 0;
	int ret = 0;
	int num_fds = 0;
	int event_size = 0;
	unsigned long data = 0;
	struct epoll_event event[EPOLL_EVENT_MAX];	

	event_size = sizeof(event);
	for (; event_size != 0;) {
		memset(event, 0, event_size);
		num_fds = epoll_wait(
				   global_epoll_data.epoll_fd,
				   event, 
				   EPOLL_EVENT_MAX, 
				   1000);
		GINFO("num_fds = %d", num_fds);
		if (num_fds < 0) {
			if (errno == EINTR)
				break;
			PANIC("epoll wait error :%s\n", strerror(errno));
		}
		ret = epoll_check_duration();
		if (ret == OK)
			break;
		//real timer end check
		//ret = rt_end_check();
		for (i = 0; i < num_fds; i++) {
			if (thread_test_exit() == OK)  {
				event_size = 0;
				break;	
			}
			data = (unsigned long)event[i].data.ptr;
			if ((event[i].events & EPOLLIN || event[i].events & EPOLLERR) && 
					(global_epoll_data.oper.epoll_recv)){
				global_epoll_data.oper.epoll_recv(data);
			}
			else if ((event[i].events & EPOLLOUT) && 
					(global_epoll_data.oper.epoll_send)) 
				global_epoll_data.oper.epoll_send(data);
			/*else if ((event[i].events & EPOLLERR) && 
					(global_epoll_data.oper.epoll_err))  {
				GINFO("epoll error:%s\n", strerror(errno));
				global_epoll_data.oper.epoll_err(errno, data);
			}*/
		}
	}
}

static int
epoll_uninit()
{
	close(global_epoll_data.epoll_fd);
	
	return OK;
}

int
epoll_init()
{
	memset(&global_epoll_data, 0, sizeof(global_epoll_data));
	global_epoll_data.epoll_fd = epoll_create(1000000);
	if (global_epoll_data.epoll_fd < 0) 
		PANIC("create epoll error : %s\n", strerror(errno));

	return uninit_register(epoll_uninit);
}

MOD_INIT(epoll_init);
