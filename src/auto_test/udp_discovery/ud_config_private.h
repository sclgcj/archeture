#ifndef UD_CONFIG_PRIVATE_H
#define UD_CONFIG_PRIVATE_H


#define UD_DEVICE_SERVER 1
#define UD_DEVICE_CLIENT 2
struct ud_config {
	int device_type;
	unsigned int ip;
	unsigned short port;
};

int
ud_config_setup();

struct ud_config *
ud_config_get();

#endif
