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
#include "ops_json.h"
#include "ops_db.h"
#include "cmd_processor.h"

static int send_syscmd(uint8_t fn, uint8_t cmd, uint8_t* json_string)
{
	struct msg_t req_msg;
	struct msg_t res_msg;
	uint32_t msg_size = sizeof(struct msg_t);
	struct ops_net_t* net = get_net_instance();
	struct ops_log_t* log = get_log_instance();
	int uds_rsp = -1;

	memset(&req_msg, 0, msg_size);
	memset(&res_msg, 0, msg_size);

	req_msg.fn = fn;
	req_msg.cmd = cmd;
	req_msg.data_size = strlen(json_string);
	log->debug(0x01, __FILE__, __func__, __LINE__, "cli str len %ld", req_msg.data_size);
	strncpy(req_msg.data, json_string, req_msg.data_size);

	uds_rsp = net->uds_client_send_and_recv(SOCKET_PATH_WWW, &req_msg, &res_msg);

	return uds_rsp;
}

void* task_timer(void* ptr)
{
    // TODO : workaround 
    sleep(1);// This is used to wait cmd task ready.

    struct ops_log_t* log = get_log_instance();
    struct ops_json_t* json = get_json_instance();
    struct ops_db_t* db = get_db_instance();
    //struct ops_misc_t* misc = get_misc_instance();
    uint8_t db_val[DBVALLEN];
    uint8_t db_val2[DBVALLEN];
    uint8_t* str_ptr = NULL;
    int ops_fn = CMD_FN_RSVD;
    int ops_cmd = CMD_NO_RSVD;
    int count = 0;
    int i = 0;
    uint16_t db_ret = 0;

    memset(&db_val[0], 0, DBVALLEN);
    db_ret = db->get_val("sysinit_count", &db_val[0]);
    if(db_ret) {
        json_reader_t* db_reader = json->create_json_reader(&db_val[0]);
        count = json->get_json_array_count(db_reader);
        for(i=0;i<count;i++){
            json_reader_t* array_reader = json->get_json_array_object_by_index(db_reader, i);
            str_ptr = json->get_json_string(array_reader, NULL, "");
            memset(&db_val2[0], 0, DBVALLEN);
            db->get_val(str_ptr, &db_val2[0]);

            json_reader_t* storage_reader = json->create_json_reader(&db_val2[0]);
            ops_fn = json->get_json_int(storage_reader, "ops_fn", CMD_FN_RSVD);
            ops_cmd = json->get_json_int(storage_reader, "ops_cmd", CMD_NO_RSVD);
            send_syscmd(ops_fn, ops_cmd, &db_val2[0]);
        }
    }
    while(1) {
	    log->debug(0x01, __FILE__, __func__, __LINE__, "");
	    sleep(1);
    }

    return NULL;
}

