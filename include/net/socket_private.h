#ifndef SOCKET_H
#define SOCKET_H

#include "std_comm.h"
#include "create_private.h"

int
create_socket(
	int proto,
	char *unix_path,
	struct in_addr addr,
	unsigned short port,
	struct create_socket_option *option,
	int *sock
);

int
tcp_connect(
	int		sock,
	struct in_addr  addr,
	unsigned short  server_port
);

int
tcp_accept(
	int sock
);

int
udp_connect(
	int		sock,
	struct in_addr  addr,
	unsigned short	server_port
);

int
udp_accept(
	int sock
);

int
unix_tcp_connect(
	int	sock,
	char	*path
);

int
unix_tcp_accept(int sock);

int
unix_udp_connect(
	int	sock,
	char	*path
);

int
unix_udp_accept(int sock);

int
block_fd_set(
	int fd
);


#endif
