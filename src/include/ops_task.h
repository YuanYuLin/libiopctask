#ifndef OPS_TASK_H
#define OPS_TASK_H

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

struct ops_task_t {
	void (*init) (void);
	void (*show_all) (void);
};

struct ops_task_t *get_task_instance();
void del_task_instance();
#endif
