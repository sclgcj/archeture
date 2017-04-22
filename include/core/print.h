#ifndef PRINT_H
#define PRINT_H 1


/**
 * PRINT() - used to print info with current time, file, line
 * @fmt:	output format
 * @args:	arguments related to format
 *
 * This macro is used to print the current info to the specified 
 * file. It will print the current print time, and print the 
 * line, the file, the function where it is use.This is a simple
 * decision. In downstring, we need a more flexible module to 
 * decide what fp is pointing to.
 *
 * Return: no
 */
#define PRINT(fmt, args...) do{ \
	struct tm *tm = NULL; \
	time_t t = 0; \
	t = time(NULL); \
	tm = localtime(&t); \
	fprintf(stderr, "[%d-%d-%d %d:%d:%d %s:%s:%d] -- "fmt, \
			tm->tm_year + 1900, tm->tm_mon+1, tm->tm_mday, \
			tm->tm_hour, tm->tm_min, tm->tm_sec, __FILE__, \
			__FUNCTION__, __LINE__, ##args); \
}while(0)


/**
 * PANIC() - print the message and exit the progtam
 * @fmt		output format
 * @args	arguments related to format
 *
 * This macro is used for output the message that can't repair. Call it 
 * will terminate the program after the 
 *
 * Return: no
 */
#define PANIC(fmt, args...) do{ \
	struct tm *tm = NULL; \
	time_t t = 0; \
	t = time(NULL); \
	tm = localtime(&t); \
	fprintf(stderr, "[%d-%d-%d %d:%d:%d %s:%s:%d] -- "fmt, \
			tm->tm_year + 1900, tm->tm_mon+1, tm->tm_mday, \
			tm->tm_hour, tm->tm_min, tm->tm_sec, __FILE__, \
			__FUNCTION__, __LINE__, ##args); \
	exit(-1); \
}while(0)


#endif


