#include <pthread.h>

#include "ops_task.h"
#include "ops_mq.h"

extern void* task_main_queue(void* ptr);
extern void* task_www_recv(void* ptr);
extern void* task_www_send(void* ptr);

#define MAX_TASK	2 
static pthread_t task_id[MAX_TASK];

static void init(void)
{
	pthread_create(&task_id[0], NULL, task_main_queue, NULL);
	pthread_create(&task_id[1], NULL, task_www_recv, NULL);
	pthread_create(&task_id[2], NULL, task_www_send, NULL);
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
