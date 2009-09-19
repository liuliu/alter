#ifndef GUARD_cluscom_daemon_h
#define GUARD_cluscom_daemon_h

#include "cluscom.h"

class ClusCom_Daemon
{
	friend class SP_ST_Dispatch;
	friend class SP_ST_Reply;
	friend class SE_TP_Interface;
	private:
		class SP_ST_Dispatch : public frl_socket_pipe
		{
			private:
				virtual apr_status_t send_after(char* buf, apr_size_t len, apr_uint32_t status);
				ClusCom_Daemon* _manager;
			public:
				SP_ST_Dispatch(ClusCom_Daemon* manager, apr_uint32_t maximum, apr_pool_t* mempool)
				: _manager(manager),
				  frl_socket_pipe(maximum, mempool)
				{ /* ... */ }
		};

		class SP_ST_Reply : public frl_socket_pipe
		{
			private:
				virtual apr_status_t recv_before(char** buf, apr_size_t* len);
				virtual apr_status_t recv_after(char* buf, apr_size_t len);
				ClusCom_Daemon* _manager;
				frl_slab_pool_t* _rep_pool;
			public:
				SP_ST_Reply(ClusCom_Daemon* manager, apr_uint32_t maximum, apr_pool_t* mempool)
				: _manager(manager),
				  frl_socket_pipe(maximum, mempool)
				{
					frl_slab_pool_create(&_rep_pool, mempool, maximum, frl_reply_size+SIZEOF_POINTER);
				}
				virtual ~SP_ST_Reply()
				{
					frl_slab_pool_destroy(_rep_pool);
				}
		};

		class SE_TP_Interface : public frl_server_event
		{
			friend class SP_ST_Dispatch;
			friend class SP_ST_Reply;
			private:
				virtual apr_status_t recv_before(char** buf, apr_size_t* len);
				virtual apr_status_t recv_send(char** buf, apr_size_t* len, frl_notify_t* notify);
				virtual apr_status_t send_after(char* buf, apr_size_t len);
				ClusCom_Daemon* _manager;
				frl_slab_pool_t* _req_pool;
			public:
				SE_TP_Interface(ClusCom_Daemon* manager, apr_uint32_t maximum, apr_pool_t* mempool)
				: _manager(manager),
				  frl_server_event(maximum, mempool)
				{
					frl_slab_pool_create(&_req_pool, mempool, maximum, frl_request_size+SIZEOF_POINTER);
				}
				virtual ~SE_TP_Interface()
				{
					frl_slab_pool_destroy(_req_pool);
				}
		};
		frl_slab_pool_t* _rep_pool;
		char* _child_addr;

		apr_thread_t* _dispatcher_thd;
		apr_thread_t* _receive_reply_thd;
		SP_ST_Dispatch* _dispatcher;
		SP_ST_Reply* _reply;
		SE_TP_Interface* _interface;
	public:
		ClusCom_Daemon(char* child_addr, apr_uint32_t maximum, apr_pool_t* _mempool)
		: _child_addr(child_addr),
		  mempool(_mempool)
		{
			_interface = new SE_TP_Interface (this, maximum, _mempool);
			_dispatcher = new SP_ST_Dispatch (this, maximum, _mempool);
			_reply = new SP_ST_Reply (this, maximum, _mempool);
		  	apr_atomic_init(_mempool);
		}
		int spawn(int n);
		int hold();
		~ClusCom_Daemon()
		{
			delete _reply;
			delete _dispatcher;
			delete _interface;
		}
		apr_pool_t* mempool;
};

#endif
