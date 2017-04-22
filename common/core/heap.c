#include "std_comm.h"
#include "err.h"
#include "heap.h"
#include "print.h"

#include "list.h"

/*
 * 该模块类似于一个优先级队列，采用的是最小堆的实现方法，目的在于
 * 帮助简化定时器的检测。当前的定时器实现，是检测所有的节点，这样
 * 子虽然目前满足需求，但是总感觉不是很好，因此参考了libevent的介
 * 绍，思考之后决定尝试使用最小堆。经过测试，2016-8-15 的版本，在
 * 对100万的数据进行处理的时候，插入删除的效率感觉还行，基本上都在
 * 1s之内完成了操作，但是感觉还是不是很满意。 在性能比较好的测试机
 * 上进行测试，1000W的处理效率阔以保持在2s以内，这还是让人感觉非常
 * 兴奋，看来是开始的处理时间是因为虚拟机的本身的性能比较低造成的。
 */
struct heap_node {
	unsigned long user_data;
	struct heap_node *parent, *right, *left, *prev;
	struct list_head node, traversal_node;
};

struct heap_head {
	struct heap_node *last;
	struct heap_node *root;
};

struct heap_data {
	int (*user_cmp_func)(unsigned long new, unsigned long old);
	struct list_head head;
	pthread_mutex_t heap_mutex;
	struct heap_head heap_head;
};


static int
heap_default_cmp(
	unsigned long new,
	unsigned long old
)
{
	//默认采用最小堆
	if (new > old)
		return OK;
	return ERR;
}

heap_handle_t
heap_create(
	int (*user_cmp_func)(unsigned long new, unsigned long old)
)
{
	struct heap_data *heap_data = NULL;	

	heap_data = (struct heap_data *)calloc(1, sizeof(*heap_data));
	if (!heap_data) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	INIT_LIST_HEAD(&heap_data->head);
	pthread_mutex_init(&heap_data->heap_mutex, NULL);
	if (user_cmp_func)
		heap_data->user_cmp_func = user_cmp_func;
	else
		heap_data->user_cmp_func = heap_default_cmp;
	heap_data->heap_head.root = (struct heap_node*)calloc(
						1, sizeof(*heap_data->heap_head.root));
	heap_data->heap_head.root->user_data = (unsigned long)-1;
	if (!heap_data->heap_head.root) {
		pthread_mutex_destroy(&heap_data->heap_mutex);
		FREE(heap_data);
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		return NULL;
	}
	heap_data->heap_head.last = heap_data->heap_head.root;
	list_add_tail(&heap_data->heap_head.root->node, &heap_data->head);

	return (heap_handle_t)heap_data;
}

static void
heap_swap_val(
	struct heap_node *heap1,
	struct heap_node *heap2
)
{
	unsigned long tmp = 0;

	tmp = heap1->user_data;
	heap1->user_data = heap2->user_data;
	heap2->user_data = tmp;
}

static void
heap_add_adjust(
	struct heap_data *heap_data,
	struct heap_node *heap_node
)
{
	int ret = 0;
	struct heap_node *tmp = heap_node;

	if (!heap_node->parent)
		return;

	while(tmp != heap_data->heap_head.root) {
		if (tmp->parent && tmp->parent->user_data != -1) {
			ret = heap_data->user_cmp_func(tmp->user_data, tmp->parent->user_data);
			if (ret != OK) {
				heap_swap_val(tmp, tmp->parent);
				tmp = tmp->parent;
				continue;
			}
		}
		break;
	}
}

static void
heap_del_adjust(
	struct heap_data *heap_data,
	struct heap_node *root
)
{
	int ret = 0;
	struct heap_node *tmp = NULL, *child = NULL;

	tmp = root;
	
	while (tmp->right || tmp->left) {
		if ((tmp->right && tmp->right->user_data != (unsigned long)-1) && 
		     (tmp->left && tmp->left->user_data != (unsigned long)-1)) {	
			ret = heap_data->user_cmp_func(
					tmp->right->user_data, 
					tmp->left->user_data);
			if (ret == OK)
				child = tmp->left;
			else 
				child = tmp->right;
		} else {
			if (tmp->right && 
					(tmp->right->user_data != (unsigned long)-1))
				child = tmp->right;
			else
				child = tmp->left;
		}
		if (child->user_data == (unsigned long)-1 || 
		    tmp->user_data == (unsigned long)-1)
			break;
		ret = heap_data->user_cmp_func(tmp->user_data, child->user_data);
		if (ret == OK) {
			heap_swap_val(tmp, child);
			tmp = child;
			continue;
		}
		break;
	}
}

unsigned long
heap_node_add(
	heap_handle_t handle,
	unsigned long    user_data
)
{
	unsigned long ret = 0;
	struct heap_data *heap_data = NULL;
	struct heap_node *right_node = NULL, *left_node = NULL, *heap_node = NULL;

	if (!handle) {
		ERRNO_SET(PARAM_ERROR);
		return OK;
	}

	heap_data = (struct heap_data*)handle;

	pthread_mutex_lock(&heap_data->heap_mutex);
	heap_node = list_entry(heap_data->head.next, struct heap_node, node);
	list_del_init(&heap_node->node);	
	if (heap_node->user_data == (unsigned long)-1) {
		heap_node->user_data = user_data;
		heap_node->prev = heap_data->heap_head.last;
		heap_data->heap_head.last = heap_node;
		/*加入添加队列尾*/
		list_add_tail(&heap_node->node, &heap_data->head);
		heap_add_adjust(heap_data, heap_node);
		goto out;
	} 

	left_node = (struct heap_node *)calloc(1, sizeof(*left_node));
	if (!left_node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		goto err;
	}
	left_node->user_data = user_data;
	left_node->parent = heap_node;
	heap_node->left = left_node;
	left_node->prev = heap_data->heap_head.last;
	heap_data->heap_head.last = left_node;
	heap_add_adjust(heap_data, left_node);
	right_node = (struct heap_node *)calloc(1, sizeof(*right_node));
	if (!right_node) {
		ERRNO_SET(NOT_ENOUGH_MEMORY);
		goto err;
	}
	right_node->parent = heap_node;
	right_node->user_data = (unsigned long)-1;
	heap_node->right = right_node;
	list_add_tail(&left_node->node, &heap_data->head);
	list_add(&right_node->node, &heap_data->head);
	ret = OK;
	goto out;
err:
	ret = ERR;
out:
	pthread_mutex_unlock(&heap_data->heap_mutex);
	return OK;
}

static void
heap_node_del( 
	struct heap_data *heap_data
)
{
	struct heap_node *heap_node = NULL, *last = NULL;

	last = heap_data->heap_head.last;
	heap_node = heap_data->heap_head.root;
	if (!heap_data->heap_head.last) 
		return;
	//交换根结点和最后一个结点的值
	heap_swap_val(last, heap_node);	
	last->user_data = (unsigned long)-1;

	// 把最后一个结点从添加队列中删除，此时它应该是最末尾的一个，
	// 然后再添加到添加队列的头，这样它就会被第一个处理，处理
	// 完之后会被再次添加进队列尾部
	list_del_init(&last->node);
	list_add(&last->node, &heap_data->head);

	//将last结点指向最后结点的上一个结点。
	if (last->prev && last != heap_data->heap_head.root) {
		heap_data->heap_head.last = last->prev;
		heap_data->heap_head.last->prev = last->prev->prev;
		heap_del_adjust(heap_data, heap_node);
	}
}

int
heap_root_data_get(
	heap_handle_t handle,
	unsigned long    *user_data
)
{
	struct heap_data *heap_data = NULL;

	if (!handle || !user_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	heap_data = (struct heap_data *)handle;

	pthread_mutex_lock(&heap_data->heap_mutex);
	if (!heap_data->heap_head.root) {
		ERRNO_SET(NO_HEAP_DATA);
		pthread_mutex_unlock(&heap_data->heap_mutex);
		return ERR;
	}
	(*user_data) = heap_data->heap_head.root->user_data;
	heap_node_del(handle);
	pthread_mutex_unlock(&heap_data->heap_mutex);

	return OK;
}

int
heap_root_data_peek(
	heap_handle_t handle,
	unsigned long	 *user_data
)
{
	struct heap_data *heap_data = NULL;

	if (!handle || !user_data) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	heap_data = (struct heap_data *)handle;
	if (!heap_data->heap_head.root) {
		ERRNO_SET(NO_HEAP_DATA);
		return ERR;
	}

	pthread_mutex_lock(&heap_data->heap_mutex);
	(*user_data) = heap_data->heap_head.root->user_data;
	pthread_mutex_unlock(&heap_data->heap_mutex);

	return OK;
}

static void
heap_default_traversal(
	unsigned long user_data
)
{
//	GINFO("user_data = %ld\n", user_data);
	return;
}

int
heap_traversal(
	heap_handle_t handle,
	void (*heap_traversal)(unsigned long user_data)
)
{
	void (*tmp_traversal)(unsigned long user_data);
	struct list_head head;
	struct heap_data *heap_data = NULL;
	struct heap_node *tmp_left = NULL, *tmp_right = NULL, *tmp = NULL;

	if (!handle) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}
	if (heap_traversal)
		tmp_traversal = heap_traversal;
	else
		tmp_traversal = heap_default_traversal;
	
	heap_data = (struct heap_data *)handle;
	tmp = heap_data->heap_head.root;
	if (!tmp)
		return OK;

	INIT_LIST_HEAD(&head);
	list_add_tail(&tmp->traversal_node, &head);

	pthread_mutex_lock(&heap_data->heap_mutex);
	while (!list_empty(&head)) {
		tmp = list_entry(head.next, struct heap_node, traversal_node);
		list_del_init(head.next);
		if (tmp->user_data == (unsigned long)-1)
			goto next;
		tmp_traversal(tmp->user_data);
next:
		if (tmp->left) 
			list_add_tail(&tmp->left->traversal_node, &head);
		if (tmp->right)
			list_add_tail(&tmp->right->traversal_node, &head);
	}
	pthread_mutex_unlock(&heap_data->heap_mutex);

	return OK;
}

int
heap_destroy(
	heap_handle_t handle,
	void (*user_data_destroy)(unsigned long user_data)
)
{
	int count = 0;
	void (*tmp_traversal)(unsigned long user_data);
	struct list_head head;
	struct heap_data *heap_data = NULL;
	struct heap_node *tmp_left = NULL, *tmp_right = NULL, *tmp = NULL;

	if (!handle) {
		ERRNO_SET(PARAM_ERROR);
		return ERR;
	}

	heap_data = (struct heap_data *)handle;
	tmp = heap_data->heap_head.root;
	if (!tmp)
		return OK;

	INIT_LIST_HEAD(&head);
	list_add_tail(&tmp->traversal_node, &head);

	pthread_mutex_lock(&heap_data->heap_mutex);
	while (!list_empty(&head)) {
		tmp = list_entry(head.next, struct heap_node, traversal_node);
		list_del_init(head.next);
		if (tmp->user_data == (unsigned long)-1) {
			goto out;
		}
		if (user_data_destroy)
			user_data_destroy(tmp->user_data);
out:
		if (tmp->left) 
			list_add_tail(&tmp->left->traversal_node, &head);
		if (tmp->right)
			list_add_tail(&tmp->right->traversal_node, &head);
		tmp->right = NULL;
		tmp->left = NULL;
		list_del_init(&tmp->node);
		FREE(tmp);
	}
	pthread_mutex_unlock(&heap_data->heap_mutex);

	return OK;
}
