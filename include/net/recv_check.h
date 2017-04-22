#ifndef RECV_CHECK_H 
#define RECV_CHECK_H 1

/*
 * recv_check_start() - start the timer to check the receiving timeout
 * @name:	the name of this timer
 * @recv_timeout: new value of packet receiving timeout, can be zero for no changes
 * @user_data:  user_data
 *
 * We use parameter name to diff the different packets on the same link
 *
 * Return: 0 if successful, -1 if not and errno will be set
 */
int
recv_check_start(char *name, int recv_timeout, unsigned long user_data);


/*
 * recv_check_stop() - stop the timer to check the receiving timeout
 * @name:	the name of this timer
 * @user_data:  user_data
 *
 * We use parameter name to diff the different packets on the same link
 *
 * Return: 0 if successful, -1 if not and errno will be set
 */
int
recv_check_stop(char *name, unsigned long user_data);

#endif
