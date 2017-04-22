#ifndef ADDR_MANAGE_H
#define ADDR_MANAGE_H 1

/*
 * 原意是想使用枚举类型来实现不同地址类型的
 * 分类，后来想了一下，这样子每增加一个地址
 * 类型都要修改一下这个头文件，不是很好，因
 * 此屏蔽这个枚举，把不同协议类型的区分交由
 * manage来分配不同的id来进行区分，各个地址 
 * 类型需要提供用于获取自身id的方法。
 */

/*enum {
	ADDR_INET,
	ADDR_UNIX,
	ADDR_INET6,
	ADDR_MAX
};*/

/*
 * 目前涉及到的地址，最多的还是unix套接字和ipv4，
 * 但是考虑到后期可能会扩展到ipv6上，因此这里使用
 * data[0] 来时实现变长数组，以满足不同地址结构。
 * 原意是想把所有结构地址都填进去，但是想想还是算
 * 了，感觉不怎么灵活，因为目前还不知道具体会使用
 * 多少地址结构，不想以后在增加的时候就修改已有的
 * 代码。data[0]所对应的结构根据不同类型不同， 不
 * 同协议的id有manage分配，各个地址类型需要自己实
 * 现一个对外的接口，用于获取其自身的这个id。每个
 * 地址类型通过manage提供的encode, create, decode,
 * destroy等函数来进行操作
 */


struct address {
	int addr_type;
	char data[0];
};

#endif
