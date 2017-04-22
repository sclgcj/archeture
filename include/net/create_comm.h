#ifndef CREATE_COMM_H
#define CREATE_COMM_H 

int
event_set(int event, unsigned long user_data);

int
peer_info_get(unsigned long user_data, unsigned int *peer_addr, unsigned short *peer_port);

int
local_info_get(unsigned long user_data, 
		  unsigned int  *local_addr, 
		  unsigned short *local_port);

#endif
