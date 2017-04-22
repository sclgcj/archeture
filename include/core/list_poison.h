#ifndef __ST_NATT__LIST_POISON_H__BY_SONGTAO_ON_2015_12_29__15_16__
#define __ST_NATT__LIST_POISON_H__BY_SONGTAO_ON_2015_12_29__15_16__

#define POISON_POINTER_DELTA 0

/*
 * These are non-NULL pointers that will result in page faults
 * under normal circumstances, used to verify that nobody uses
 * non-initialized list entries.
 */
#define LIST_POISON1  ((void *) 0x00100100 + POISON_POINTER_DELTA)
#define LIST_POISON2  ((void *) 0x00200200 + POISON_POINTER_DELTA)

#endif
