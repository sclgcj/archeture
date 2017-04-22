#ifndef CREATE_H
#define CREATE_H

#include "addr_manage.h"
#include "epoll.h"
#include <net/if.h>


enum {
	LINK_DEL = 1,
	LINK_DESTROY
};


/*
 * Different protocol may have different link config.
 * We hope upstream to determine how they configure.
 */
/*struct create_link_config {
	int total_link;
	unsigned int start_ip;
	unsigned int end_ip;
	unsigned int server_ip;
	unsigned short start_port;
	unsigned short end_port;
	unsigned short server_port;
};*/

struct create_link_oper{
	/*
	 * prepare_data_get() - get prepare data from upstream
	 * @port_map_cnt:	 the port number used at present	
	 * @data:		 a pointer to the upstream data
	 *
	 * We provide port_map_cnt parameter for upstreams to allow them to allocate data 
	 * for different port. Sometimes the first link is client, but the second or other
	 * link may server. Upstream may allocate different data types because of different
	 * situations.
	 *
	 * Return: 0 if successful, -1 if not
	 */
	int (*prepare_data_get)(int port_map_cnt, unsigned long data);
	/*
	 * create_flow_ctrl() - flow control
	 * @cur_count: current link count
	 */
	void (*create_flow_ctrl)(int cur_count);
	/*
	 * extra_data_set() - set downstream data to the upstream
	 * @extra_data:		downstream data
	 * @user_data:		user data
	 *
	 * Sometimes upstreams will call the downstream function, and some functions 
	 * may need some data that only operated by downstream. In order to hide these
	 * details to the upstreams, we provide this interface for upstream to store 
	 * this kind of data.
	 */
	void (*extra_data_set)(unsigned long extra_data, unsigned long user_data);
	/*
	 * create_link() - create a link
	 *
	 * Return: 0 if successful, -1 if not
	 */
	int (*create_link)(
		//	int sock, 
		//	unsigned int   ip,
		//	unsigned short port, 
			unsigned long data);
	/*
	 * err_handle() - user defined err handle function when something wrong in a link
	 * @reason:	the error resason
	 * @user_data:	user used data
	 *
	 * Return: 0 - nothing to do; -1 sometion wrong; LINK_DEL - del link from epoll;
	 *	   LINK_DESTROY - destroy the link, this will destroy all the data of 
	 *	   this link containing downstream data and upstream data.
	 */
	int (*err_handle)(int reason, unsigned long user_data);
	/*
	 * send_data() - user defined send function
	 * @sock:	socket
	 * @user_data： user data
	 * @send_buf:	the buffer storing the sending data
	 * @send_len:	the length of the sending data
	 * @addr:	udp used peer address
	 *
	 * Return: 0 if successful, -1 if not
	 */
	/*int (*send_data)(
			 unsigned long user_data
			 );*/
	/*
	 * recv_data() - user define recv fucntion
	 * @sock:	socket
	 * @user_data:  user data
	 * @recv_buf:	the buffer storing the received data, its space is allocated by user
	 * @recv_len:	the length of the received data
	 * @addr:	udp used peer address
	 *
	 * Return: 0 if there is more data to be received, -1 means something wrong, >0 means
	 *	   receiving over.
	 *
	 * We will store the received data if recv_data() return 0. Upstream can store the 
	 * data all by themselves by returning size.
	 */
	int (*recv_data)(char *ptr, int size, unsigned long user_data);
	/*
	 * handle_data() - user defined function to dispose recvd data
	 * @user_data:	user data
	 *
	 * Return 0 if successful, -1 if not
	 */
	int (*handle_data)(unsigned long data);
	/*
	 * connected_func() - user defined connect function
	 * @user_data:  user_data,
	 * @event:	the event after connected func, user defined
	 * @addr:	udp used peer address
	 *
	 * Return: 0 if successful, -1 if not
	 */
	int (*connected_func)(
			unsigned long user_data);

	/*
	 * accept_func() - user defined accept function
	 * @addr:	peer addr
	 * @user_data:  user defined data for a connection
	 *
	 * By default, we set the new socket to EPOLLIN event because if we want to 
	 * send some data we can call  
	 *
	 * Return: 0 if successful, -1 if not
	 */
	int (*accept_func)(
			struct address *addr,
			unsigned long user_data);
	/*
	 * udp_accept_func() - user defined accept function
	 * @sock:	socket
	 * @user_data:  user defined data for a connection
	 * @addr:	peer addr
	 *
	 * Return: 0 if successful, -1 if not
	 */
/*	int (*udp_accept_func)(
			unsigned long extra_data,
			struct address *addr,
			int *event, 
			unsigned long user_data);*/
	/*
	 * udp_recv() - user defined udp recv function
	 */
	int (*udp_recv_data)(
			char *ptr, 
			int size,
			struct address *addr,
			unsigned long user_data);
	/*
	 * harbor_func() - user defined function to orgnize harbor data
	 * @user_data:  user data
	 *
	 * This function can just be used to orgnize harbor data and set event to EVENT_WRITE
	 * to send this packet by downstream. Of course, it can send this packet itself.
	 *
	 * Return: 0 if successful, -1 if not
	 */
	int (*harbor_func)(unsigned long user_data);

	/*
	 * result_func() - dispose the link result
	 * @user_data:	user_data
	 *
	 * In test,we need this to handle each link's result.
	 */
	void (*result_func)(unsigned long user_data);
	/*
	 * data_destroy() - destroy user_data
	 * @user_data:	user_data
	 */
	void (*data_destroy)(unsigned long user_data);
};

/*
 * link_create() - create the link module
 * @proto:		application protocol, such as http, dns etc.
 * @link_count:		the total link number that this protocol need
 * @user_data_size:	user defined structure data size
 * @oper:		create operations, set struct create_link_oper
 *
 * We decide to hide all the details of the downstreams. This means upstreams 
 * can not get any downstream infomation directly. Upstream gives downstream 
 * its data size and downstream will create the its data memory, so the 
 * upstream needn't care about its structure memory. What upstream need to 
 * do is to manage the data its structure has.We recommend that server and 
 * client use the same structure. If not we recommend to set user_data_size 
 * the bigger one. 
 *
 * In order to support multiple protocols in one program, we decide to add 
 * proto parameter to distinguish different protocols. At first, we consider 
 * that we will nerver face with multi-protocol situations, but we are 
 * wrong. In some situations we really need multiple protocols to help us 
 * realize the function. Adding link_count is also for this reason. Use config
 * file to determine different protocols' total link is not suitable for 
 * multi-protocol situations. We hope that different protocol determine their
 * own total link num.
 * 
 * Return: 0 if successful, -1 if not
 */
/*int
link_create(
	char *proto,
	int user_data_size,
	struct create_link_oper *oper
);*/

/*
 * create_link_new() - create a new link, can be a client connecting to server or a server 
 *			  to accept client links
 * @proto:	link protocol
 * @link_type:  server or client
 * @server_path: unix socket path
 * @server_ip： server address, if 0 will use the server ip configured in configure file
 * @server_port: server port, if 0 will use the server port configured in configure file
 * @extra_data: data set by extra_data_set function 
 * @oper:	the operation of this socket. We just imagine that the new socket will have 
 *		the same operation of the first socket. If the oper is not null, we will 
 *		use it to replace the old one. We just imagine this case, a server or client 
 *		may have more than one socket, an every socket will have different operation,
 *		so we should provide an interface to create sockets with their own operation.
 *
 * We provide this functions because some protocals may create a new link when the first 
 * link created(such as ftp). This function should be used after link_create.
 *
 * Return: 0 if successful, -1 if not and errno will be set
 */
int 
create_link_new(
	char *proto,
	char *server_path,
	unsigned int   server_ip,
	unsigned short server_port,
	unsigned long extra_data,
	struct create_link_oper *oper
);

/*#define CREATE_TCP_UNIX_SERVER(path, data, oper) \
	create_link_new(PROTO_TCP, DEV_SERVER, path, tmp, 0, data, oper);

#define CREATE_UDP_UNIX_SERVER(path, data, oper) \
	create_link_new(PROTO_UDP, DEV_SERVER, path, tmp, 0, data, oper);

#define CREATE_TCP_UNIX_CLIENT(path, data, oper) \
	create_link_new(PROTO_TCP, DEV_CLIENT, path, 0, 0, data, oper); 

#define CREATE_UDP_UNIX_CLIENT(path, data, oper) \
	create_link_new(PROTO_UDP, DEV_CLIENT, path, 0, 0, data, oper); 

#define CREATE_TCP_SERVER(sip, sport, data, oper) \
	create_link(PROTO_TCP, DEV_SERVER, NULL, sip, sport, data, oper); 

#define CREATE_UDP_SERVER(sip, sport, data, oper) \
	create_link(PROTO_TCP, TCP_DEV_SERVER, NULL, sip, sport, data, oper); 

#define CREATE_TCP_CLIENT(sip, sport, data, oper) \
	create_link(PROTO_TCP, DEV_CLIENT, NULL, sip, sport, data, oper);

#define CREATE_UDP_CLIENT(sip, sport, data, oper) \
	create_link(PROTO_UDP, DEV_CLIENT, NULL, sip, sport, data, oper);*/
/*
 * create_link_recreate() - recreate a link
 * @flag:	if use the same port to create a new link, 1 - use the same port, 0 - not
 * @close_link: if close the link：	
 *		0 - close, but not destroy the structure data
 *		1 - close and destroy the  structure data
 * @server_path: unix socket path
 * @server_ip:	new connections' server address, if 0, the configured server ip will be used
 * @server_port:new connections' server port, if 0, the configured server pot will be used
 * @extra_data:	data set by extra_data_set function
 * @oper:	the operation of this socket. We just imagine that the new socket will have 
 *		the same operation of the first socket. If the oper is not null, we will 
 *		use it to replace the old one. We just imagine this case, a server or client 
 *		may have more than one socket, an every socket will have different operation,
 *		so we should provide an interface to create sockets with their own operation.
 *
 * In fact, we don't want to provide this kind of function, because similar function can use
 * create_link_new to implement. However, create_link_new will create a new data structure,
 * and sometimes we may hope to use current structure for a new connection. Of course, we imagine
 * the recreated link using the same link type of the old one. Upstreams should provide the new 
 * link's server_ip and server_port, if they are 0, we will use the configured values.upstream
 * should tell us if we should use the port_map port to create a new link or just use the same
 * port to create a new link. if flag == 1, close_link == 2 is forbidding. This function should 
 * be used after link_create.
 *
 * Return: 0 if successful, -1 if not and errno will be set
 */
int
create_link_recreate(
	int flag,
	int close_link,
	char *server_path,
	unsigned int   server_ip,
	unsigned short server_port,
	unsigned long extra_data,
	struct create_link_oper *oper
);

/*
 * link_create_start() - start the process
 *  
 */
int 
link_create_start(
	char *app_proto,
	int user_data_size,
	struct create_link_oper *oper
);

#define RECREATE_TCP_UNIX_SERVER(flag, cf, path, data, oper) \
	create_link_recreate(flag, cf, path, tmp, 0, data, oper);

#define RECREATE_UDP_UNIX_SERVER(flag, cf, path, data, oper) \
	create_link_recreate(flag, cf, path, tmp, 0, data, oper);

#define RECREATE_TCP_UNIX_CLIENT(flag, c, path, data, oper) \
	create_link_recreate(flag, cf, path, 0, 0, data, oper); 

#define RECREATE_UDP_UNIX_CLIENT(flag, cf, path, data, oper) \
	create_link_recreate(flag, cf, path, 0, 0, data, oper); 

#define RECREATE_TCP_SERVER(flag, cf, sip, sport, data, oper) \
	create_link_recreate(flag, cf, NULL, sip, sport, data, oper); 

#define RECREATE_UDP_SERVER(flag, cf, sip, sport, data, oper) \
	create_link_recreate(flag, cf, NULL, sip, sport, data, oper); 

#define RECREATE_TCP_CLIENT(flag, cf, sip, sport, data, oper) \
	create_link_recreate(flag, cf, NULL, sip, sport, data, oper);

#define RECREATE_UDP_CLIENT(flag, cf, sip, sport, data, oper) \
	create_link_recreate(flag, cf, NULL, sip, sport, data, oper);

#endif
