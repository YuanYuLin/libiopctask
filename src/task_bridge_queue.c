#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "ops_mq.h"
#include "ops_log.h"
#include "ops_task.h"
#include "ops_cmd.h"

void* task_bridge_queue(void* ptr)
{
    struct queue_msg_t queue_req;
    struct queue_msg_t queue_res;
    struct msg_t* req = &queue_req.msg;
    struct msg_t* res = &queue_res.msg;
    struct ops_mq_t* mq = get_mq_instance();
    struct ops_log_t* log = get_log_instance();
    struct ops_cmd_t* cmd = get_cmd_instance();
    uint8_t cmd_status = CMD_STATUS_NOT_FOUND;

	while(1) {
	        memset((uint8_t*)&queue_req, 0, sizeof(struct queue_msg_t));
        	memset((uint8_t*)&queue_res, 0, sizeof(struct queue_msg_t));
		mq->get_from(QUEUE_NAME_MAIN, &queue_req);         

		queue_res.index = queue_req.index;
		queue_res.magic = queue_req.magic;
		strcpy(queue_res.src, queue_req.dst);
		strcpy(queue_res.dst, queue_req.src);

        	log->debug(0x01, "------------------------------------\n");
	        log->debug(0x01, "get MsgQ(%d)[%x] %s -> %s : fn, cmd = %x, %x\n", queue_req.index, queue_req.magic, queue_req.src, queue_req.dst, req->fn, req->cmd);
		//process_cmd(req, res);
        	log->debug(0x01, "%x, %x, %x, %s\n", req->fn, req->cmd, req->status, req->data);
		cmd_status = cmd->process(req, res);
        	log->debug(0x01, "%x, %x, %x, %s\n", res->fn, res->cmd, res->status, res->data);
		log->debug(0x01, "-%x-----------------------------------\n", cmd_status);

	        mq->set_to(queue_res.dst, &queue_res);
	}

	return NULL;
}

