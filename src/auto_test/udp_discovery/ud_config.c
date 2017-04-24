#include "comm.h"
#include "ud_err.h"
#include "ud_config_private.h"

static struct ud_config global_ud_config;

int
ud_config_setup() 
{
	char multi_type[32] = { 0 };
	cJSON *obj = NULL;    

	fprintf(stderr, "hhhhhh\n");
	obj = config_read_get("udp_discovery");
	CR_STR(obj, "multi_type", multi_type);
	if (!strcmp(multi_type, "server"))
		global_ud_config.device_type = UD_DEVICE_SERVER;
	else 
		global_ud_config.device_type = UD_DEVICE_CLIENT;
	CR_IP(obj, "multi_addr", global_ud_config.ip);
	CR_USHORT(obj, "multi_port", global_ud_config.port);

	
	return UD_OK;	
}  
	
struct ud_config *
ud_config_get()
{
	return &global_ud_config;
}
