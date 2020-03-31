#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include "ops_mq.h"
#include "ops_net.h"
#include "ops_log.h"
#include "ops_task.h"
#include "ops_cmd.h"
#include "ops_json.h"
#include "ops_db.h"
#include "cmd_processor.h"
#ifdef SUPPORT_RFB
#include "ops_rfb.h"
#endif
#ifdef SUPPORT_DRM
#include "ops_drm.h"
#endif

static struct drm_dev_t* g_drm_dev;

static int init_dri()
{
#ifdef SUPPORT_DRM
	struct ops_drm_t* drm = get_drm_instance();
	struct ops_log_t* log = get_log_instance();
	struct ops_db_t* db = get_db_instance();
	struct ops_json_t* json = get_json_instance();
	uint8_t db_val[DBVALLEN];
	uint8_t *dri_path = NULL;
	struct drm_dev_t *drm_head = NULL;
	int drm_fd = -1;

	memset(&db_val[0], 0, DBVALLEN);
	db->get_val("dri_cfg", &db_val[0]);
	json_reader_t* db_reader = json->create_json_reader(&db_val[0]);
	dri_path = json->get_json_string(db_reader, "path", "");

	drm_fd = drm->open(dri_path);
	if(drm_fd < 0) {
		return -1;
	}
	drm_head = drm->find_dev(drm_fd);
	if (drm_head == NULL) {
		return -2;
	}

	/*
	log->debug(0x01, "available connector(s)\n\n");
	for (drm_dev = drm_head; drm_dev != NULL; drm_dev = drm_dev->next) {
		log->debug(0x01, "connector id:%d\n", drm_dev->conn_id);
		log->debug(0x01, "\tencoder id:%d crtc id:%d fb id:%d\n", drm_dev->enc_id, drm_dev->crtc_id, drm_dev->fb_id);
		log->debug(0x01, "\twidth:%d height:%d\n", drm_dev->width, drm_dev->height);
	}
	*/

	g_drm_dev = drm_head;
	log->debug(0x01, __FILE__, __func__, __LINE__, "connector id:%d, w x h: %d, %d", g_drm_dev->conn_id, g_drm_dev->width, g_drm_dev->height);
	log->debug(0x01, __FILE__, __func__, __LINE__, "\tencoder id:%d crtc id:%d fb id:%d", g_drm_dev->enc_id, g_drm_dev->crtc_id, g_drm_dev->fb_id);

	drm->setup(drm_fd, g_drm_dev);

	log->debug(0x01, __FILE__, __func__, __LINE__, "connector id:%d, w x h: %d, %d", g_drm_dev->conn_id, g_drm_dev->width, g_drm_dev->height);
	log->debug(0x01, __FILE__, __func__, __LINE__, "\tencoder id:%d crtc id:%d fb id:%d", g_drm_dev->enc_id, g_drm_dev->crtc_id, g_drm_dev->fb_id);
#endif
	return drm_fd;
}

static void* process_rfb_drm_test(void* arg)
{
	int idx = 0;
	struct ops_log_t *log = get_log_instance();
	struct drm_dev_t *drm_dev = (struct drm_dev_t*)g_drm_dev;
	log->debug(0x01, __FILE__, __func__, __LINE__, "%d x %d", drm_dev->width, drm_dev->height);
	while(1) {
		idx = idx % 5;
		for(int y=0; y<drm_dev->height; y++) {
			for(int x=0; x<drm_dev->width; x++) {
				uint32_t color = (y << 24) + (x << 16) + (x * y * idx);
				*(drm_dev->buf + (y * drm_dev->width) + x) = color;
			}
		}
		idx++;
		sleep(3);
	}
	return NULL;
}

static int get_rfb_count()
{
	uint8_t db_val[DBVALLEN] = {0};
	int count = 0;
	struct ops_db_t* db = get_db_instance();
	struct ops_json_t* json = get_json_instance();
	struct ops_log_t* log = get_log_instance();
	memset(&db_val[0], 0, DBVALLEN);
	db->get_val("rfb_count", &db_val[0]);
	json_reader_t* db_reader = json->create_json_reader(&db_val[0]);
	count = json->get_json_array_count(db_reader);
	log->debug(0x01, __FILE__, __func__, __LINE__, "%d", count);
	return count;
}

static uint8_t* get_rfb_by_index(uint8_t index)
{
	uint8_t db_val[DBVALLEN] = {0};
	struct ops_db_t* db = get_db_instance();
	struct ops_json_t* json = get_json_instance();
	struct ops_log_t* log = get_log_instance();
	uint8_t *str_ptr = NULL;
	memset(&db_val[0], 0, DBVALLEN);
	db->get_val("rfb_count", &db_val[0]);
	log->debug(0x01, __FILE__, __func__, __LINE__, "%d , %s", index, db_val);
	json_reader_t* db_reader = json->create_json_reader(&db_val[0]);
	str_ptr = json->get_json_array_string_by_index(db_reader, index, "");
	log->debug(0x01, __FILE__, __func__, __LINE__, "%s", str_ptr);
	return str_ptr;
}

struct rfb_arg_t {
	uint8_t index;
	uint8_t action;
};

#define RFB_INFO_STOP		0
#define RFB_INFO_START		1
struct rfb_conn_info_t {
	uint8_t running_status;
	pthread_t pid;
};

struct rfb_conn_info_t rfb_conn_info[] = {
	{ RFB_INFO_STOP, -1 },
	{ RFB_INFO_STOP, -1 },
	{ RFB_INFO_STOP, -1 },
	{ RFB_INFO_STOP, -1 },
	{ RFB_INFO_STOP, -1 },
	{ RFB_INFO_STOP, -1 },
	{ RFB_INFO_STOP, -1 },
	{ RFB_INFO_STOP, -1 },
	{ RFB_INFO_STOP, -1 },
	{ RFB_INFO_STOP, -1 }
};

#define COLUM	3
#define SCALE_FACTOR	0.33
static void* process_rfb_conn_start(void* arg)
{
	struct rfb_arg_t *rfb_arg = (struct rfb_arg_t*)arg;
	uint8_t idx = rfb_arg->index;
	//uint8_t action = rfb_arg->action;
	int socket_fd = -1;
	struct ops_log_t *log = get_log_instance();
	struct ops_db_t *db = get_db_instance();
	struct ops_json_t *json = get_json_instance();
	struct ops_rfb_t *rfb = get_rfb_instance();
	struct server_init_t si;
	struct framebuffer_dev_t fb_dev_rfb;
	struct framebuffer_dev_t fb_dev_phy;

	uint8_t db_val[DBVALLEN] = { 0 };
	uint8_t *rfb_name = get_rfb_by_index(idx);
	memset(&db_val[0], 0, DBVALLEN);
	db->get_val(rfb_name, &db_val[0]);
	log->debug(0x01, __FILE__, __func__, __LINE__, "rfb idx : %d, act : %d", rfb_arg->index, rfb_arg->action);
	log->debug(0x01, __FILE__, __func__, __LINE__, "rfb key:%s, val=%s", rfb_name, db_val);
	json_reader_t *rfb_reader = json->create_json_reader(&db_val[0]);
	uint8_t* rfb_host = json->get_json_string(rfb_reader, "host", "127.0.0.1");
	int rfb_port = json->get_json_int(rfb_reader, "port", 5919);

	log->debug(0x01, "rfb conn %s::%d\n", rfb_host, rfb_port);
        socket_fd = rfb->create_socket(rfb_host, rfb_port);
	if (rfb->handshake(socket_fd, &si) < 0) {
		log->error(0xFF, __FILE__, __func__, __LINE__, "handleshake failed....");
		rfb->close_socket(socket_fd);
		return NULL;
	}
	if (rfb->request_entire_screen(socket_fd, &si) < 0) {
		log->error(0xFF, __FILE__, __func__, __LINE__, "request content failed....");
		rfb->close_socket(socket_fd);
		return NULL;
	}
        fb_dev_rfb.width = si.fb_width;
	fb_dev_rfb.height = si.fb_height;
	fb_dev_rfb.x_pos = 0;
	fb_dev_rfb.y_pos = 0;
	fb_dev_rfb.fb_ptr = malloc(si.fb_width * si.fb_height * (si.fb_pixel_format.bit_per_pixel / 8));
	fb_dev_rfb.depth = DEPTH;
	fb_dev_rfb.bpp = si.fb_pixel_format.bit_per_pixel;

	do {
		fb_dev_phy.width = (g_drm_dev->width * SCALE_FACTOR);
		fb_dev_phy.height = (g_drm_dev->height * SCALE_FACTOR);
		fb_dev_phy.x_pos = ((idx % COLUM) + 1)	* (g_drm_dev->width * SCALE_FACTOR);
		fb_dev_phy.y_pos = ((idx / COLUM) + 15)	* (g_drm_dev->height * SCALE_FACTOR);
		fb_dev_phy.fb_ptr = g_drm_dev->buf;
		fb_dev_phy.depth = DEPTH;
		fb_dev_phy.bpp = BPP;

		if (rfb->processor(socket_fd, &si, &fb_dev_rfb) < 0) {
			log->error(0x01, __FILE__, __func__, __LINE__, "processor error....");
			break;
		}
		
		float scale_width = (float)fb_dev_phy.width / (float)fb_dev_rfb.width;
		float scale_height = (float)fb_dev_phy.height / (float)fb_dev_rfb.height;

		for(uint16_t pos_y=0;pos_y<fb_dev_phy.height;pos_y++){
			for(uint16_t pos_x=0;pos_x<fb_dev_phy.width;pos_x++){
				uint16_t rfb_x = (uint16_t)(((float)pos_x)/scale_width + 0.5);
				uint16_t rfb_y = (uint16_t)(((float)pos_y)/scale_height + 0.5);
				log->debug(0x01, __FILE__, __func__, __LINE__, "[%f-x-%d/%d:%f-y-%d/%d] = [%d:%d] = [%d:%d]", scale_width, fb_dev_phy.width, fb_dev_rfb.width, scale_height, fb_dev_phy.height, fb_dev_rfb.height, pos_x, pos_y, rfb_x, rfb_y);
				uint32_t px = *(fb_dev_rfb.fb_ptr + ((rfb_y * fb_dev_rfb.width) + rfb_x));
				*(g_drm_dev->buf + (((fb_dev_phy.y_pos + pos_y) * g_drm_dev->width) + (fb_dev_phy.x_pos + pos_x))) = px;
			}
		}

		if (rfb->request_changed_screen(socket_fd, &si) < 0) {
			log->error(0x01, __FILE__, __func__, __LINE__, "request content failed....");
			break;
		}
	} while(rfb_conn_info[idx].running_status == RFB_INFO_START);

	rfb->close_socket(socket_fd);

	return NULL;
}

static int process_rfb_msg(struct rfb_arg_t* rfb_arg, struct req_rfb_msg_t* req_rfb_msg, struct res_rfb_msg_t* res_rfb_msg)
{
	struct ops_log_t *log = get_log_instance();
	log->debug(0x01, __FILE__, __func__, __LINE__, "rfb req_msg idx:%d, act:%d", req_rfb_msg->index, req_rfb_msg->action);
	res_rfb_msg->status = 0x0;
	res_rfb_msg->index = 0x01;
	res_rfb_msg->action = 0x09;
	rfb_arg->index = req_rfb_msg->index;
	rfb_arg->action = req_rfb_msg->action;
	pthread_t pid;

	int slot_count = get_rfb_count();
	if( rfb_arg->index >= slot_count) {
		log->error(0xFF, __FILE__, __func__, __LINE__, "rfb %d/%d error: index not valid", rfb_arg->index, slot_count );
		return sizeof(struct res_rfb_msg_t);
	}

	log->debug(0x01, __FILE__, __func__, __LINE__, "rfb idx:%d, act:%d", rfb_arg->index, rfb_arg->action);
	if(rfb_arg->action == RFB_DRM_TEST) {
		pthread_create(&pid, NULL, &process_rfb_drm_test, (void*)rfb_arg);
	}else if(rfb_arg->action == RFB_CONN_START) {
		if(rfb_conn_info[rfb_arg->index].running_status == RFB_INFO_START) {
			log->error(0xFF, __FILE__, __func__, __LINE__, "slot is running [%d]", rfb_arg->index);
			return sizeof(struct res_rfb_msg_t);
		}
		rfb_conn_info[rfb_arg->index].running_status = RFB_INFO_START;
		pthread_create(&pid, NULL, &process_rfb_conn_start, (void*)rfb_arg);
		rfb_conn_info[rfb_arg->index].pid = pid;
	}else if(rfb_arg->action == RFB_CONN_STOP) {
		rfb_conn_info[rfb_arg->index].running_status = RFB_INFO_STOP;
	}

	res_rfb_msg->status = 0x0;
	res_rfb_msg->index = 0x01;
	res_rfb_msg->action = 0x09;
	return sizeof(struct res_rfb_msg_t);
}

void* task_rfb(void* ptr)
{
	struct ops_log_t *log = get_log_instance();
	struct ops_mq_t* mq = get_mq_instance();
	struct queue_msg_t queue_req;
	struct queue_msg_t queue_res;
	struct msg_t* req = &queue_req.msg;
	struct msg_t* res = &queue_res.msg;
	struct req_rfb_msg_t* req_rfb_msg = (struct req_rfb_msg_t*)&req->data;
	struct res_rfb_msg_t* res_rfb_msg = (struct res_rfb_msg_t*)&res->data;
	int drm_fd = -1;
	struct rfb_arg_t rfb_arg;
	drm_fd = init_dri();
	log->info(0x01, __FILE__, __func__, __LINE__, "drm fd %d, w[%d] x h[%d]", drm_fd, g_drm_dev->width, g_drm_dev->height);

	while(1) {
		memset((uint8_t*)&queue_req, 0, sizeof(struct queue_msg_t));
		memset((uint8_t*)&queue_res, 0, sizeof(struct queue_msg_t));
		mq->get_from(QUEUE_NAME_RFBSERVER, &queue_req);

		res->data_size = process_rfb_msg(&rfb_arg, req_rfb_msg, res_rfb_msg);
		queue_res.index = queue_req.index;
		queue_res.magic = queue_req.magic;
		strcpy(queue_res.src, queue_req.dst);
		strcpy(queue_res.dst, queue_req.src);

		mq->set_to(QUEUE_NAME_RFBCLIENT, &queue_res);
	}

    return NULL;
}

