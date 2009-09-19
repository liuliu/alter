#ifndef GUARD_multi_sock_h
#define GUARD_multi_sock_h

#include "apr.h"
#include "apr_portable.h"
#include "apr_thread_mutex.h"
#include "apr_thread_cond.h"
#include "apr_queue.h"
#include "cluscom-config.h"

struct frl_task_t
{
	apr_socket_t* socket;
	bool status;
#if APR_HAS_THREADS
	apr_thread_mutex_t* mutex;
	apr_thread_cond_t* cond;
#endif
};

void* thread_accept( apr_thread_t* thd, void* data );
void* thread_read( apr_thread_t* thd, void* data );

class frl_multi_sock
{
	friend void* thread_accept( apr_thread_t* thd, void* data );
	friend void* thread_read( apr_thread_t* thd, void* data );
	private:
		apr_threadattr_t* _thd_attr;
		apr_thread_t* _accept;
		apr_thread_t** _readers;
		apr_queue_t* _reader_queue;
		apr_pool_t* _sockpool;
		frl_task_t* _reader_tasks;
		virtual int proc( apr_socket_t* reader, int who ) = 0;
	public:
		frl_multi_sock( apr_uint32_t _port, apr_pool_t* _mempool )
		: port(_port),
		  mempool(_mempool)
		{
			apr_pool_create( &_sockpool, mempool );
			apr_threadattr_create( &_thd_attr, _mempool );
		}
		virtual ~frl_multi_sock()
		{
			apr_queue_term( _reader_queue );
			apr_pool_destroy( _sockpool );
		}
		apr_thread_t* spawn( int n );
		apr_uint32_t port;
		apr_pool_t* mempool;
};

#endif
