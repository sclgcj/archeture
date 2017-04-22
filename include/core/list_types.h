#ifndef __ST_NATT__LIST_TYPES_H__BY_SONGTAO_ON_2015_12_29__15_19__
#define __ST_NATT__LIST_TYPES_H__BY_SONGTAO_ON_2015_12_29__15_19__

#define prefetch(x) __builtin_prefetch(x)

struct list_head
{
    struct list_head *next, *prev;
};

struct hlist_head
{
    struct hlist_node *first;
};

struct hlist_node
{
    struct hlist_node *next, **pprev;
};

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
//#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define container_of(ptr, type, memory) \
		((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#endif

