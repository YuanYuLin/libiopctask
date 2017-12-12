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
#include "ops_log.h"
#include "ops_task.h"

static int socket_fd = -1;
//static int epoll_fd = -1;

struct uds_session_t {
	volatile uint8_t is_used;
	uint32_t index;
	uint32_t magic;
	struct sockaddr_un cli_addr;
	socklen_t cli_addr_len;
};

static struct uds_session_t session[MAX_CLIENT];

static int bind_socket(void) 
{
	struct sockaddr_un addr;
	struct ops_log_t* log = get_log_instance();
	
	socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if(socket_fd < 0) {
		log->error(0x01, "socket error : %s\n", strerror(errno));
		return -1;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	log->debug(0x01, "serv create socket path: %s\n", SOCKET_PATH);
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);
	unlink(SOCKET_PATH);
	if(bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		log->error(0x01, "bind error : %s\n", strerror(errno));
		return -2;
	}

	return 0;
}

static void close_socket(void) 
{
	if(socket_fd != -1) 
		close(socket_fd);
}

static int handle_client(struct uds_session_t* uds_session) 
{
	//struct uds_msg_t req_uds_msg;
	struct queue_msg_t queue_msg;// = &req_uds_msg->queue_msg;
	struct msg_t* req = &queue_msg.msg;
	uint32_t rc = 0;
	struct ops_log_t* log = get_log_instance();
	struct ops_mq_t* mq = get_mq_instance();
	//uint16_t i = 0;

	memset(&queue_msg, 0, sizeof(struct queue_msg_t));
	//memset(&uds_session->cli_addr, 0, sizeof(struct sockaddr_un));
	rc = recvfrom(socket_fd, req, sizeof(struct msg_t), 0, (struct sockaddr*)&uds_session->cli_addr, &uds_session->cli_addr_len);
	log->debug(0x01, "serv read count = %ld, %s, %ld\n", rc, uds_session->cli_addr.sun_path, uds_session->cli_addr_len);

	if(rc < 0) {
		log->error(0x01, "handle client error : <= 0 \n");
		close(socket_fd);
		return -1;
	}
	/*
	if(rc != sizeof(struct msg_t)) {
		log->error(0x01, "handle client error : %ld >= msg size\n", rc);
		close(socket_fd);
		return -2;
	}
	*/

	if(req->data_size > MAX_MSG_DATA_SIZE) {
		log->error(0x01, "data size > %ld\n", MAX_MSG_DATA_SIZE);
		close(socket_fd);
		return -3;
	}

	log->debug(0x01, "www req- fn: %x, cmd: %x, ds: %ld\n", req->fn, req->cmd, req->data_size);
	/*
	for(i=0;i<req->data_size;i++) {
		log->debug(0x01, "%x,", req->data[i]);
	}
	log->debug(0x01, "\n");
	*/
	queue_msg.index = uds_session->index;
	queue_msg.magic = uds_session->magic;
	strcpy(queue_msg.src, QUEUE_NAME_WWW);
	strcpy(queue_msg.dst, QUEUE_NAME_MAIN);

	mq->set_to(queue_msg.dst, &queue_msg);
	return 0;
}

void* task_www_recv(void* ptr)
{
	uint32_t i = 0;
	uint32_t magic = 0;
	struct uds_session_t *uds = NULL;
	for(i=0;i<MAX_CLIENT;i++) {
		uds = &session[i];
		memset(uds, 0, sizeof(struct uds_session_t));
		uds->is_used = 0;
		uds->cli_addr_len = sizeof(struct sockaddr_un);
		uds->index = i;
	}
	uds = NULL;

	if(bind_socket() < 0) {
		printf("bind socket error\n");
		return NULL;
	}

	while(1) {
		for(i=0;i<MAX_CLIENT;i++) {
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

	close_socket();

	return NULL;
}

void* task_www_send(void* ptr)
{
    struct queue_msg_t queue_msg;
	struct msg_t* res = &queue_msg.msg;
    struct uds_session_t *uds = NULL;
    uint16_t msg_hdr_size = sizeof(struct msg_t) - MAX_MSG_DATA_SIZE;
	uint32_t msg_size = 0;
	uint32_t rc = 0;
	uint32_t wc = 0;
	struct ops_mq_t* mq = get_mq_instance();
	struct ops_log_t* log = get_log_instance();
	//uint16_t i = 0;

	while(1) {
        	memset((uint8_t*)&queue_msg, 0, sizeof(struct queue_msg_t));
		rc = mq->get_from(QUEUE_NAME_WWW, &queue_msg);
		log->debug(0x01, "get from %s, %s, %d, %ld , %ld\n", queue_msg.src, queue_msg.dst, queue_msg.index, queue_msg.magic, rc);
		msg_size = msg_hdr_size + queue_msg.msg.data_size;
	log->debug(0x01, "www res- fn: %x, cmd: %x, ds: %ld\n", res->fn, res->cmd, res->data_size);
	/*
	for(i=0;i<res->data_size;i++) {
		log->debug(0x01, "%x,", res->data[i]);
	}
	log->debug(0x01, "\n");
	*/
		uds = &session[queue_msg.index];
		log->debug(0x01, "send path : %s, %ld\n", uds->cli_addr.sun_path, uds->cli_addr_len);
	wc = sendto(socket_fd, (void*)&queue_msg.msg, msg_size, 0, (struct sockaddr*)&uds->cli_addr, uds->cli_addr_len);
	if(wc < 0) {
		log->error(0x01, "handle client send error \n");
		log->debug(0x01, "sendto error : %s\n", strerror(errno));
	}
	log->debug(0x01, "serv Send size %ld\n", wc);
	uds->is_used = 0;
	}
	return NULL;
}

