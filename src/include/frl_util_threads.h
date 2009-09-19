/*************************
 * Fotas Runtime Library *
 * ***********************
 * leader-follower thread pool model
 * author: Liu Liu
 */
#ifndef GUARD_frl_util_threads_h
#define GUARD_frl_util_threads_h

#include "frl_base.h"
#include "frl_queue.h"

void* thread_leader(apr_thread_t* thd, void* data);
void* thread_consumer(apr_thread_t* thd, void* data);
void* thread_producer(apr_thread_t* thd, void* data);

class frl_threads;

struct frl_thread_stat_t
{
	apr_time_t start;
	apr_uint32_t handled;
	apr_uint32_t failed;
	apr_status_t status;
};

struct frl_thread_config_t
{
	apr_uint32_t no;
	frl_threads* master;
};

struct frl_thread_select_t
{
	union {
		apr_int32_t no;
		void* pointer;
	};
#if APR_HAS_THREADS
	apr_thread_mutex_t* mutex;
	apr_thread_cond_t* select;
#endif
};

const apr_uint32_t SIZEOF_FRL_THREAD_STAT_T = sizeof(frl_thread_stat_t);
const apr_uint32_t SIZEOF_FRL_THREAD_CONFIG_T = sizeof(frl_thread_config_t);
const apr_uint32_t SIZEOF_FRL_THREAD_SELECT_T = sizeof(frl_thread_select_t);

class frl_threads
{
	friend void* thread_leader(apr_thread_t* thd, void* data);
	friend void* thread_consumer(apr_thread_t* thd, void* data);
	friend void* thread_producer(apr_thread_t* thd, void* data);
	private:
		virtual apr_status_t execute(void* pointer) = 0;
		
		frl_thread_stat_t* thd_stat;
		frl_thread_config_t* thd_config;
		
		apr_thread_t** thd_worker;
		apr_thread_t* thd_master;
		apr_threadattr_t* thd_attr;

		apr_thread_mutex_t* mutex;
		apr_thread_cond_t* select;

		frl_thread_select_t* leader;
		frl_thread_select_t* consumers;
		apr_queue_t* consumer_queue;

		frl_queue_t* task_queue;
		
		apr_pool_t* mempool;
		
		frl_thread_model_u model;
		frl_lock_u lock;

		bool destroyed;
	public:
		frl_threads(frl_thread_model_u _model, frl_lock_u _lock, apr_pool_t* _mempool)
		: model(_model),
		  lock(_lock)
		{
			destroyed = 0;
			apr_pool_create(&mempool, _mempool);
		}
		virtual ~frl_threads()
		{
			destroyed = 1;
			apr_pool_destroy(mempool);
		}
		apr_status_t spawn(apr_uint32_t min, apr_uint32_t max);
		apr_status_t assign(void* pointer);
		apr_status_t wait();
		frl_thread_stat_t state(apr_uint32_t no);
		apr_uint32_t waiters;
		apr_uint32_t min;
		apr_uint32_t max;
		apr_uint32_t open;
};

#endif
