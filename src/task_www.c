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
#include <microhttpd.h>

#include "ops_mq.h"
#include "ops_net.h"
#include "ops_log.h"
#include "ops_task.h"
#include "ops_cmd.h"
#include "ops_db.h"

#define HTTPD_FLAGS		MHD_USE_EPOLL_INTERNALLY | MHD_USE_EPOLL | MHD_USE_DEBUG
#define HTTPD_PORT		80
#define HTTPD_HOST		"0.0.0.0"
static struct MHD_Daemon *service = NULL;

struct desc_param_t {
	struct MHD_Connection *con;
	char *url;
	uint8_t method;
	uint8_t version;
	char *upload_data;
	size_t upload_data_size;
	uint8_t val[DBVALLEN];
	uint8_t content_type[DBVALLEN];
	//uint8_t *val;
	struct MHD_PostProcessor *postprocessor;
	//uint8_t buf[DBVALLEN];
};

#define METHOD_GET	((uint8_t)0x01)
#define METHOD_POST	((uint8_t)0x02)

#define HTTP_11		((uint8_t)0x01)

struct www_desc_t {
	uint8_t method;
	uint8_t url[32];
	int (*handler)(void *cls, void **con_cls, struct desc_param_t* param);
};

static int handle_hello(void *cls, void **con_cls, struct desc_param_t* param)
{
    struct ops_log_t* log = get_log_instance();
    const char *page = "<html><body>Hello, browser!</body></html>";
    //struct MHD_Response *response;
    int ret = MHD_YES;
    log->debug(0x01, "%s, %d\n", __func__, __LINE__);
    memcpy(param->val, page, strlen(page));
    strcpy(param->content_type, "text/html; charset=utf-8");

    return ret;
}

static int handle_get_dao(void *cls, void **con_cls, struct desc_param_t* param)
{
	int ret = MHD_YES;
	//struct MHD_Response *response;
	struct ops_log_t* log = get_log_instance();
	struct msg_t req_msg;
	struct msg_t res_msg;
	uint32_t msg_size = sizeof(struct msg_t);
	struct ops_net_t* net = get_net_instance();

	const char* key = MHD_lookup_connection_value (param->con, MHD_GET_ARGUMENT_KIND, "key");
	log->debug(0x01, "%s, %d\n", __func__, __LINE__);
	log->debug(0x01, "%s\n", param->url);
	log->debug(0x01, "%s\n", key);
	memset(param->val, 0, DBVALLEN);
	/* content begin */
	memset(&req_msg, 0, msg_size);
	memset(&res_msg, 0, msg_size);
	req_msg.fn = CMD_FN_2;
	req_msg.cmd = CMD_NO_4;
	sprintf(req_msg.data, "{\"%s\":\"%s\"}", KV_KEY, key);
	req_msg.data_size = strlen(req_msg.data);
	net->uds_client_send_and_recv(&req_msg, &res_msg);
	memcpy(param->val, res_msg.data, res_msg.data_size);
	/* content end */

	log->debug(0x01, "%d-%s\n", strlen(param->val), param->val);
	strcpy(param->content_type, "application/json; charset=utf-8");

	return ret;
}

static int iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
		const char *filename, const char *content_type, const char *transfer_encoding, 
		const char *data, uint64_t off, size_t size)
{
    //struct desc_param_t *con_info = (struct desc_param_t*)coninfo_cls;
	struct ops_log_t* log = get_log_instance();
	log->debug(0x01, "key:%s", key);
	return MHD_YES;
}

static int handle_set_dao(void *cls, void **con_cls, struct desc_param_t* param)
{
	int ret = MHD_YES;
	//struct MHD_Response *response;
	struct ops_log_t* log = get_log_instance();
	struct msg_t req_msg;
	struct msg_t res_msg;
	uint32_t msg_size = sizeof(struct msg_t);
	struct ops_net_t* net = get_net_instance();

	const char* key = MHD_lookup_connection_value (param->con, MHD_GET_ARGUMENT_KIND, "key");
	memset(&req_msg, 0, msg_size);
	memset(&res_msg, 0, msg_size);
	if(param->upload_data_size == 0) {
		log->debug(0x01, "%s-%s-%d\n", __FILE__, __func__, __LINE__);
		/* content begin */
		memset(param->val, 0, DBVALLEN);
		req_msg.fn = CMD_FN_2;
		req_msg.cmd = CMD_NO_4;
		sprintf(req_msg.data, "{\"%s\":\"%s\"}", KV_KEY, key);
		//req_msg[0]=(uint8_t)strlen(key);
		//memcpy(&req_msg[1], key, strlen(key));
		req_msg.data_size = strlen(req_msg.data);
		net->uds_client_send_and_recv(&req_msg, &res_msg);
		log->debug(0x1, "%d,%d:%s\n", strlen(res_msg.data), res_msg.data_size, res_msg.data);
		memcpy(param->val, res_msg.data, res_msg.data_size);
		strcpy(param->content_type, "application/json; charset=utf-8");
		/* content end */
	} else {
		log->debug(0x01, "%s-%s-%d\n", __FILE__, __func__, __LINE__);
		/* content begin */
		req_msg.fn = CMD_FN_2;
		req_msg.cmd = CMD_NO_5;
		sprintf(req_msg.data, "{\"%s\":\"%s\", \"%s\":%s}", KV_KEY, key, KV_VAL, param->upload_data);
		req_msg.data_size = strlen(req_msg.data);
		net->uds_client_send_and_recv(&req_msg, &res_msg);
		log->debug(0x1, "%d,%d:%s\n", strlen(res_msg.data), res_msg.data_size, res_msg.data);
		memcpy(param->val, res_msg.data, res_msg.data_size);
		/* content end */
		param->upload_data_size = 0;
	}
	return ret;
}

static int handle_run_ops(void *cls, void **con_cls, struct desc_param_t* param)
{
	int ret = MHD_YES;
	//struct MHD_Response *response;
	struct ops_log_t* log = get_log_instance();

	struct msg_t req_msg;
	struct msg_t res_msg;
	uint32_t msg_size = sizeof(struct msg_t);
	struct ops_net_t* net = get_net_instance();

	if(param->upload_data_size == 0) {
		log->debug(0x01, "%s-%s-%d\n", __FILE__, __func__, __LINE__);
		memset(param->val, 0, DBVALLEN);
		/* content begin */
		log->debug(0x01, "res_msg[%d] %s\n", res_msg.data_size, res_msg.data);
		memcpy(param->val, "{\"ok\":0}", strlen("{\"ok\":0}"));
		strcpy(param->content_type, "application/json; charset=utf-8");
		/* content end */
	} else {
		log->debug(0x01, "%s-%s-%d\n", __FILE__, __func__, __LINE__);
		/* content begin */
		memset(&req_msg, 0, msg_size);
		memset(&res_msg, 0, msg_size);
		req_msg.fn = CMD_FN_2;
		req_msg.cmd = CMD_NO_1;
		req_msg.data_size = param->upload_data_size;
		memcpy(req_msg.data, param->upload_data, req_msg.data_size);
		net->uds_client_send_and_recv(&req_msg, &res_msg);
		/* content end */
		param->upload_data_size = 0;
	}
	return ret;
}

static int handle_get_ops_status(void *cls, void **con_cls, struct desc_param_t* param)
{
	int ret = MHD_YES;
	//struct MHD_Response *response;
	struct ops_log_t* log = get_log_instance();
	struct msg_t req_msg;
	struct msg_t res_msg;
	uint32_t msg_size = sizeof(struct msg_t);
	struct ops_net_t* net = get_net_instance();
	const char* status_id = MHD_lookup_connection_value (param->con, MHD_GET_ARGUMENT_KIND, "id");

	log->debug(0x01, "%s-%s-%d\n", __FILE__, __func__, __LINE__);
	/* content begin */
	memset(&req_msg, 0, msg_size);
	memset(&res_msg, 0, msg_size);
	req_msg.fn = CMD_FN_2;
	req_msg.cmd = CMD_NO_3;
	sprintf(req_msg.data, "{\"ops\":\"get_status\", \"status_id\":%d}", atoi(status_id));
	req_msg.data_size = strlen(req_msg.data);
	net->uds_client_send_and_recv(&req_msg, &res_msg);

	strcpy(param->content_type, "application/json; charset=utf-8");
	memset(param->val, 0, DBVALLEN);
	memcpy(param->val, res_msg.data, res_msg.data_size);
	/* content end */

	return ret;
}

static struct www_desc_t www_descs[] = {
	{METHOD_GET,	"/html/hello",	handle_hello},
	{METHOD_GET,	"/api/v1/dao/",	handle_get_dao},
	{METHOD_POST,	"/api/v1/dao/", handle_set_dao},
	{METHOD_POST,	"/api/v1/ops",	handle_run_ops},
	{METHOD_GET,	"/api/v1/status", handle_get_ops_status},
};

static int httpd_handler(void *cls, struct MHD_Connection *con, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls)
{
    struct ops_log_t* log = get_log_instance();
    log->debug(0x01, "url:%s, method=%s, ver=%s\n", url, method, version);
    int ret = 0;
    int idx = 0;
    int www_desc_count = sizeof(www_descs)/sizeof(struct www_desc_t);
    struct www_desc_t* www_desc = NULL;
    struct desc_param_t *con_info;
    struct MHD_Response *response;

    if( (0 == strcmp(method, "POST")) && 
		    (NULL == *con_cls) ) {
	    con_info = malloc (sizeof (struct desc_param_t));
	    if ( NULL == con_info )
		    return MHD_NO;
	    con_info->con = con;
	    con_info->url = (char*)url;
	    if(0 == strcmp(version, "HTTP/1.1")) {
		    con_info->version = HTTP_11;
	    }
	    con_info->method = METHOD_POST;
	    const char* content_len = MHD_lookup_connection_value (con, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
	    const char* content_type = MHD_lookup_connection_value (con, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);

	    if( (memcmp(content_type, "application/json;", strlen("application/json;")) == 0) && (atol(content_len) > 0)) {
		    con_info->postprocessor = NULL;
	    } else {
		    con_info->postprocessor = MHD_create_post_processor (con, 1024, iterate_post, (void *)con_info);
		    if (NULL == con_info->postprocessor) {
			    free(con_info);
			    return MHD_NO;
		    }
	    }
    } else if( (0 == strcmp(method, "GET")) && 
		    (NULL == *con_cls) ) {
	    con_info = malloc (sizeof (struct desc_param_t));
	    if ( NULL == con_info )
		    return MHD_NO;
	    con_info->con = con;
	    con_info->url = (char*)url;
	    if(0 == strcmp(version, "HTTP/1.1")) {
		    con_info->version = HTTP_11;
	    }
	    con_info->method = METHOD_GET;
    }

    if(NULL == *con_cls) {
	    *con_cls = (void *) con_info;
	    return MHD_YES;
    } else {
	    con_info = (struct desc_param_t*)(*con_cls);
	    con_info->upload_data = (char*)upload_data;
	    con_info->upload_data_size = *upload_data_size;
    }

    for(idx=0;idx<www_desc_count;idx++){
	    www_desc = &www_descs[idx];
	    if( (con_info->method == www_desc->method) && (memcmp(url, www_desc->url, strlen(www_desc->url)) == 0) ){
		    ret = www_desc->handler(cls, con_cls, con_info);
		    //*upload_data_size = con_info->upload_data_size;
		    break;
		    //return ret;
	    }
    }
    
    if(*upload_data_size == 0) {
	    response = MHD_create_response_from_buffer (strlen (con_info->val), (void *) con_info->val, MHD_RESPMEM_MUST_COPY);
	    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, con_info->content_type);
	    ret = MHD_queue_response (con, MHD_HTTP_OK, response);
	    MHD_destroy_response (response);
    }
    *upload_data_size = con_info->upload_data_size;
    log->debug(0x01, "ret = %d\n", ret);

    return MHD_YES;
}

static void request_completed(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe)
{
	struct ops_log_t* log = get_log_instance();
	struct desc_param_t *con_info = (struct desc_param_t*)(*con_cls);

	if (NULL == con_info) {
		log->debug(0x01, "con_info is NULL");
		return ;
	}

	if (con_info->method == METHOD_POST) {
		log->debug(0x01, "destroy post processor\n");
		if(con_info->postprocessor)
			MHD_destroy_post_processor (con_info->postprocessor);
	}

	log->debug(0x01, "free con_info\n");
	free (con_info);
	*con_cls = NULL;
}

void* task_www(void* ptr)
{
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(HTTPD_PORT);
	inet_pton(AF_INET, HTTPD_HOST, &(addr.sin_addr.s_addr));

	service = MHD_start_daemon(HTTPD_FLAGS, htons(HTTPD_PORT), NULL, NULL, &httpd_handler, NULL, 
			MHD_OPTION_SOCK_ADDR, (struct sockaddr*) &addr, 
			MHD_OPTION_NOTIFY_COMPLETED, &request_completed, NULL,
			MHD_OPTION_END);
	return NULL;
}
