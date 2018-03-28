#include <pthread.h>

#include "ops_task.h"
#include "ops_mq.h"

struct task_info_t {
	pthread_t pid;
	void* (*task_func)(void* ptr);
};

//extern void* task_uds_db_recv(void* ptr);
//extern void* task_uds_db_send(void* ptr);
//extern void* task_main_db_queue(void* ptr);
extern void* task_main_queue(void* ptr);
extern void* task_uds_www_recv(void* ptr);
extern void* task_uds_www_send(void* ptr);
//extern void* task_syscmd_queue(void* ptr);
extern void* task_sysinit(void* ptr);
extern void* task_www(void* ptr);

static struct task_info_t task_list[] = {
//	{ 0, task_uds_db_send },
//	{ 0, task_uds_db_recv },
//	{ 0, task_main_db_queue },

	{ 0, task_main_queue },
	{ 0, task_uds_www_send },
	{ 0, task_uds_www_recv },
//	{ 0, task_syscmd_queue },

	{ 0, task_sysinit },
	{ 0, task_www }
};

static void init(void)
{
	int list_size = sizeof(task_list)/sizeof(struct task_info_t);
	int i = 0;
	for(i=0;i<list_size;i++) {
		if(task_list[i].task_func)
			pthread_create(&task_list[i].pid, NULL, task_list[i].task_func, NULL);
	}
}

static void show_all(void)
{
}

static struct ops_task_t *obj;
struct ops_task_t *get_task_instance()
{
	if (!obj) {
		obj = malloc(sizeof(struct ops_task_t));
		obj->init = init;
		obj->show_all = show_all;
	}

	return obj;
}

void del_task_instance()
{
	if (obj)
		free(obj);
}
