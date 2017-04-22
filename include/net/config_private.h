#ifndef CONFIG_PRIVATE_H
#define CONFIG_PRIVATE_H 1

/*
 * config_handle() - handle the config file
 *
 * It is a internal function which is used by downstream program,
 * upstreams don't need to care about it. It will go through all 
 * registered config option, and call config_open to open the 
 * specific config file, config_close to close this file. It will
 * use libtoml to analyze the config file, so we rule that all 
 * the config file should be a toml file. 
 *
 * Return: 0 if successful, -1 if not and specific errno will be set
 */
int
config_handle();



#endif
