#ifndef GUARD_frl_queue_h
#define GUARD_frl_queue_h

#include "apr_thread_cond.h"
#include "frl_slab_pool.h"

struct frl_queue_entry_t {
	void* pointer;
	frl_queue_entry_t* next;
};

struct frl_queue_t {
	frl_slab_pool_t* pool;
	frl_queue_entry_t* head;
	frl_queue_entry_t* tail;
	apr_uint32_t empty_waiters;
	apr_int32_t destroyed;
	apr_thread_cond_t* not_empty; 
	apr_uint32_t nelts;
	frl_lock_u lock;
	frl_memory_u memory;
#if APR_HAS_THREADS
	apr_thread_mutex_t* mutex;
#endif
};

const apr_uint32_t SIZEOF_FRL_QUEUE_T = sizeof(frl_queue_t);
const apr_uint32_t SIZEOF_FRL_QUEUE_ENTRY_T = sizeof(frl_queue_entry_t);

APR_DECLARE(void*) frl_queue_pop(frl_queue_t* queue);
APR_DECLARE(apr_status_t) frl_queue_push(frl_queue_t* queue, void* pointer);
APR_DECLARE(void*) frl_queue_trypop(frl_queue_t* queue);
APR_DECLARE(apr_status_t) frl_queue_trypush(frl_queue_t* queue, void* pointer);
APR_DECLARE(void*) frl_queue_peek(frl_queue_t* queue);
APR_DECLARE(void) frl_queue_destroy(frl_queue_t* queue);
APR_DECLARE(apr_status_t) frl_queue_create(frl_queue_t** newqueue, apr_pool_t* mempool, apr_uint32_t capacity, frl_lock_u lock, frl_slab_pool_t* pool = 0);

#endif
