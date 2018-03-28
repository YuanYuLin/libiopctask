#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "ops_mq.h"
#include "ops_misc.h"
#include "ops_log.h"
#include "ops_task.h"
#include "ops_cmd.h"
#include "ops_json.h"

void* task_syscmd_queue(void* ptr)
{
    struct queue_msg_t qreq;
    struct msg_t* req = &qreq.msg;
    struct ops_mq_t* mq = get_mq_instance();
    struct ops_log_t* log = get_log_instance();
    struct ops_cmd_t* cmd = get_cmd_instance();
    int16_t index = -1;

	while(1) {
		memset((uint8_t*)&qreq, 0, sizeof(struct queue_msg_t));
		mq->get_from(QUEUE_NAME_SYSCMD, &qreq);

		log->debug(0x01, "sys [%x-%x]: %s\n", req->fn, req->cmd, req->data);

		index = req->cmd;
		cmd->exec_shell(index, req->data);
	}

	return NULL;
}

