#include <sys/socket.h>
#include <sys/epoll.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "ops_mq.h"
#include "ops_net.h"
#include "ops_log.h"
#include "ops_task.h"

static int socket_fd = -1;

struct uds_session_t {
	uint8_t is_used;
	uint8_t magic;
	uint16_t index;
	struct sockaddr_un cli_addr;
	socklen_t cli_addr_len;
} __attribute__ ((packed));

static struct uds_session_t session[MAX_CLIENT_WWW];

static int handle_client(struct uds_session_t* uds_session) 
{
	struct queue_msg_t queue_msg;
	struct msg_t* req = &queue_msg.msg;
	uint32_t rc = 0;
	struct ops_log_t* log = get_log_instance();
	struct ops_mq_t* mq = get_mq_instance();
	struct ops_net_t* net = get_net_instance();

	memset(&queue_msg, 0, sizeof(struct queue_msg_t));
	rc = net->uds_server_recv(socket_fd, req, &uds_session->cli_addr, &uds_session->cli_addr_len);
	log->debug(0x01, __FILE__, __func__, __LINE__, "serv read count = %ld, %s, %ld", rc, uds_session->cli_addr.sun_path, uds_session->cli_addr_len);

	if(rc < 0) {
		log->error(0xFF, __FILE__, __func__, __LINE__, "handle client error : <= 0");
		close(socket_fd);
		return -1;
	}

	if(req->data_size > MAX_MSG_DATA_SIZE) {
		log->error(0xFF, __FILE__, __func__, __LINE__, "data size > %ld", MAX_MSG_DATA_SIZE);
		close(socket_fd);
		return -3;
	}

	log->debug(0x01, __FILE__, __func__, __LINE__, "www req- fn: %x, cmd: %x, ds: %ld", req->fn, req->cmd, req->data_size);

	queue_msg.index = uds_session->index;
	queue_msg.magic = uds_session->magic;
	strcpy(queue_msg.src, QUEUE_NAME_UDS_WWW);
	strcpy(queue_msg.dst, QUEUE_NAME_MAIN);

	mq->set_to(queue_msg.dst, &queue_msg);
	return 0;
}

void* task_uds_recv(void* ptr)
{
	uint32_t i = 0;
	uint32_t magic = 0;
	struct uds_session_t *uds = NULL;
	struct ops_net_t* net = get_net_instance();
	struct ops_log_t* log = get_log_instance();
	for(i=0;i<MAX_CLIENT_WWW;i++) {
		uds = &session[i];
		memset(uds, 0, sizeof(struct uds_session_t));
		uds->is_used = 0;
		uds->cli_addr_len = sizeof(struct sockaddr_un);
		uds->index = i;
	}
	uds = NULL;

	socket_fd = net->uds_server_create(SOCKET_PATH_WWW);
	if(socket_fd < 0) {
		log->error(0xFF, __FILE__, __func__, __LINE__, "bind socket error");
		return NULL;
	}

	while(1) {
		for(i=0;i<MAX_CLIENT_WWW;i++) {
			uds = &session[i];
			if(uds->is_used) {
				continue;
			} else {
				uds->is_used = 1;
				magic += 1;
				memset(&uds->cli_addr, 0, sizeof(struct sockaddr_un));
				uds->magic = magic;
				handle_client(uds);
			}
		}
	}

	net->uds_close(socket_fd);

	return NULL;
}

void* task_uds_send(void* ptr)
{
    struct queue_msg_t queue_msg;
	struct msg_t* res = &queue_msg.msg;
    struct uds_session_t *uds = NULL;
	uint32_t rc = 0;
	uint32_t wc = 0;
	struct ops_mq_t* mq = get_mq_instance();
	struct ops_log_t* log = get_log_instance();
	struct ops_net_t* net = get_net_instance();

	while(1) {
        	memset((uint8_t*)&queue_msg, 0, sizeof(struct queue_msg_t));
		rc = mq->get_from(QUEUE_NAME_UDS_WWW, &queue_msg);
		log->debug(0x01, __FILE__, __func__, __LINE__, "get from %s, %s, %d, %ld , %ld", queue_msg.src, queue_msg.dst, queue_msg.index, queue_msg.magic, rc);
		log->debug(0x01, __FILE__, __func__, __LINE__, "www res- fn: %x, cmd: %x, ds: %ld", res->fn, res->cmd, res->data_size);
		uds = &session[queue_msg.index];
		log->debug(0x01, __FILE__, __func__, __LINE__, "send path : %s, %ld", uds->cli_addr.sun_path, uds->cli_addr_len);
		wc = net->uds_server_send(socket_fd, &queue_msg.msg, &uds->cli_addr, uds->cli_addr_len);
		if(wc < 0) {
			log->error(0xFF, __FILE__, __func__, __LINE__, "handle client send error: %s", strerror(errno));
		}
		log->debug(0x01, __FILE__, __func__, __LINE__, "serv Send size %ld", wc);
		uds->is_used = 0;
	}
	return NULL;
}

