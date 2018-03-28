#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "ops_mq.h"
#include "ops_net.h"
#include "ops_log.h"
#include "ops_task.h"
#include "ops_cmd.h"
#include "cmd_processor.h"

static int send_syscmd(uint8_t fn, uint8_t cmd, uint8_t* json_string)
{
	struct msg_t req_msg;
	struct msg_t res_msg;
	uint32_t msg_size = sizeof(struct msg_t);
	struct ops_net_t* net = get_net_instance();
	int uds_rsp = -1;

	memset(&req_msg, 0, msg_size);
	memset(&res_msg, 0, msg_size);

	req_msg.fn = fn;
	req_msg.cmd = cmd;
	req_msg.data_size = strlen(json_string);
	strncpy(req_msg.data, json_string, req_msg.data_size);

	uds_rsp = net->uds_client_send_and_recv(&req_msg, &res_msg);

	return uds_rsp;
}

void* task_sysinit(void* ptr)
{
    send_syscmd(CMD_FN_2, CMD_NO_1, "{\"ops\":\"mount_hdd\", \"dev\":\"all\"}");
    send_syscmd(CMD_FN_2, CMD_NO_1, "{\"ops\":\"up_ifc\", \"ifc\":\"all\"}");
    //send_syscmd(CMD_FN_2, CMD_NO_1, "{\"ops\":\"set_hostname\"}");

    //send_syscmd(CMD_FN_2, CMD_NO_1, "{\"ops\":\"start_drbd\", \"is_master\":0, \"mounted_dir\":\"/hdd/drbd\"}");

    return NULL;
}

