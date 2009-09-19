#include "cluscom-daemon.h"

apr_status_t
ClusCom_Daemon::SP_ST_Dispatch::send_after(char* buf,
					   apr_size_t len,
					   apr_uint32_t status)
{
	frl_slab_pfree(*(frl_mem_t**)(buf+frl_request_size));
	if (SEND_IS_FAIL(status))
	{
#ifdef FRL_DEBUGGING
		printf("[ClusCom_Daemon::SP_ST_Dispatch::send_after]: Send is Fail.\n");
#endif
	}
	return APR_SUCCESS;
}

apr_status_t
ClusCom_Daemon::SP_ST_Reply::recv_before(char** buf,
					 apr_size_t* len)
{
	if (*len != frl_reply_size)
	{
#ifdef FRL_DEBUGGING
		printf("[ClusCom_Daemon::SP_ST_Reply::recv_before]: Receive a Bad Package Length.\n");
#endif
		return SOCKET_PIPE_CLOSE;
	} else {
		frl_mem_t* rep_mem = frl_slab_palloc(_rep_pool);
		*buf = (char*)rep_mem->pointer;
		*(frl_mem_t**)((char*)rep_mem->pointer+frl_reply_size) = rep_mem;
	}
	return SOCKET_PIPE_CONTINUE;
}

apr_status_t
ClusCom_Daemon::SP_ST_Reply::recv_after(char* buf,
					apr_size_t len)
{
	frl_notify_t* notify = *(frl_notify_t**)buf;
	notify->buf = buf+frl_uint64_size;
	notify->len = REPLY_SIZE;
	return APR_SUCCESS;
}

apr_status_t
ClusCom_Daemon::SE_TP_Interface::recv_before(char** buf,
					     apr_size_t* len)
{
	frl_mem_t* newmem = frl_slab_palloc(_req_pool);
	*buf = (char*)newmem->pointer+frl_uint32_size;
	*(frl_mem_t**)((char*)newmem->pointer+frl_request_size) = newmem;
	*len = REQUEST_SIZE;
	return SERVER_EVENT_CONTINUE;
}

apr_status_t
ClusCom_Daemon::SE_TP_Interface::recv_send(char** buf,
					   apr_size_t* len,
					   frl_notify_t* notify)
{
	char* newbuf = *buf-frl_uint32_size;
	*(frl_notify_t**)newbuf = notify;
	notify->timeout = apr_time_now()+DEF_PROCESS_TIMEOUT;
	_manager->_dispatcher->send(newbuf, frl_request_size);
	return SERVER_EVENT_PENDING;
}

apr_status_t
ClusCom_Daemon::SE_TP_Interface::send_after(char* buf,
					    apr_size_t len)
{
	frl_slab_pfree(*(frl_mem_t**)(buf+REPLY_SIZE));
	return SERVER_EVENT_CLOSE;
}

int
ClusCom_Daemon::spawn(int n)
{
	apr_sockaddr_t* sock_addr;
	apr_sockaddr_info_get(&sock_addr, NULL, APR_INET, DAEMON_PORT, 0, mempool);
	_interface->spawn(n, sock_addr);
	apr_sockaddr_info_get(&sock_addr, _child_addr, APR_INET, REQUEST_PORT, 0, mempool);
	_dispatcher_thd = _dispatcher->spawn(n, sock_addr, SOCKET_PIPE_SENDER);
	apr_sockaddr_info_get(&sock_addr, NULL, APR_INET, REPLY_PORT, 0, mempool);
	_receive_reply_thd = _reply->spawn(n, sock_addr, SOCKET_PIPE_RECEIVER);
	return 0;
}

int
ClusCom_Daemon::hold()
{
	apr_status_t rv;
	apr_thread_join(&rv, _receive_reply_thd);
	return rv;
}
