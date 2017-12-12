#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "ops_mq.h"
#include "ops_log.h"
#include "ops_task.h"

//uint16_t process_data(uint16_t req_data_len, uint8_t* req_data_ptr, uint8_t* res_data_ptr)
uint16_t process_data(struct msg_t* req, struct msg_t* res)
{
	//int i = 0;
	uint8_t* payload = NULL;
	uint8_t fn = 0;
	uint8_t cmd = 0;
	struct ops_log_t* log = get_log_instance();
	struct ops_mq_t* mq = get_mq_instance();
	json_reader_t* reader = mq->create_json_reader(req->data);
	json_writer_t* writer = mq->create_json_writer();

	//res->data_size = req->data_size;

	log->debug(0x01, "json ds: %ld\n", req->data_size);
	log->debug(0x01, "%s\n", req->data);
	payload = mq->get_json_string(reader, "Payload", "Test");
	log->debug(0x01, "payload:%s\n", payload);
	fn = mq->get_json_int(reader, "Fn", 0);
	log->debug(0x01, "fn:%d\n", fn);
	cmd = mq->get_json_int(reader, "Cmd", 0);
	log->debug(0x01, "cmd:%d\n", cmd);
	/*
	for(i=0;i<req->data_size;i++) {
		res->data[i] = toupper(req->data[i]);
	}
	*/
	mq->set_json_int(writer, "Fn", fn);
	mq->set_json_int(writer, "Cmd", cmd);
	mq->set_json_string(writer, "Payload", "Hello World");
	mq->set_json_string(writer, "ReqPayload", payload);

	res->data_size = mq->out_json_to_bytes(writer, &res->data[0]);
	log->debug(0x01, "len =%ld, data:%s\n", res->data_size, res->data);
	
	return res->data_size;
}

void* task_main_queue(void* ptr)
{
    struct queue_msg_t queue_req;
    struct queue_msg_t queue_res;
    struct msg_t* req = &queue_req.msg;
    struct msg_t* res = &queue_res.msg;
    uint8_t group;
    uint8_t cmdno;
    //uint8_t *req_data_ptr = (uint8_t*)&req->data;
    //uint8_t *res_data_ptr = (uint8_t*)&res->data;
    uint16_t req_data_len = 0;
    uint16_t res_data_len = 0;
    struct ops_mq_t* mq = get_mq_instance();
    struct ops_log_t* log = get_log_instance();
    //uint16_t i = 0;

	while(1) {
	        memset((uint8_t*)&queue_req, 0, sizeof(struct queue_msg_t));
        	memset((uint8_t*)&queue_res, 0, sizeof(struct queue_msg_t));
		mq->get_from(QUEUE_NAME_MAIN, &queue_req);         

		log->debug(0x01, "req idx, magic, src, dst: %d, %ld, %s, %s\n", queue_req.index, queue_req.magic, queue_req.src, queue_req.dst);
		log->debug(0x01, "mq req- fn: %x, cmd: %x, ds: %ld\n", req->fn, req->cmd, req->data_size);
		/*
		for(i=0;i<req->data_size;i++) {
			log->debug(0x01, "%x,", req->data[i]);
		}
		log->debug(0x01, "\n");
		*/
		queue_res.index = queue_req.index;
		queue_res.magic = queue_req.magic;
		strcpy(queue_res.src, queue_req.dst);
		strcpy(queue_res.dst, queue_req.src);

	        group = req->fn;
        	cmdno = req->cmd;
		req_data_len = req->data_size;

        	log->debug(0x01, "------------------------------------\n");
	        log->debug(0x01, "get MsgQ[%x.%x]%d\n", group, cmdno, req_data_len);
		log->debug(0x01, "------------------------------------\n");
		//res_data_len = process_data(req_data_len, req_data_ptr, res_data_ptr);
		res_data_len = process_data(req, res);
        	res->data_size = res_data_len;
	        res->fn = 0x80 | group;
		res->cmd = cmdno;
		log->debug(0x01, "res idx, magic, src, dst: %d, %ld, %s, %s\n", queue_req.index, queue_req.magic, queue_req.src, queue_req.dst);

		log->debug(0x01, "mq res- fn: %x, cmd: %x, ds: %ld\n", res->fn, res->cmd, res->data_size);
		/*
		for(i=0;i<res->data_size;i++) {
			log->debug(0x01, "%x,", res->data[i]);
		}
		log->debug(0x01, "\n");
		*/
	        mq->set_to(queue_res.dst, &queue_res);
	}

	return NULL;
}

