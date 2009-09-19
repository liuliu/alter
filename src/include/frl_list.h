#ifndef GUARD_frl_list_h
#define GUARD_frl_list_h

#include "frl_slab_pool.h"

struct frl_list_t;

struct frl_list_entry_t
{
	frl_key_t key;
	void* pointer;
	frl_list_t* list;
	frl_list_entry_t* next;
	frl_list_entry_t* prev;
};

struct frl_list_t
{
	frl_slab_pool_t* pool;
	frl_list_entry_t* head;
	frl_list_entry_t* tail;
	apr_uint32_t nelts;
	frl_lock_u lock;
	frl_memory_u memory;
#if APR_HAS_THREADS
	apr_thread_rwlock_t* rwlock;
#endif
};

struct frl_list_stat_t
{
	apr_uint32_t nelts;
	apr_uint32_t busy;
};

const apr_uint32_t SIZEOF_FRL_LIST_ENTRY_T = sizeof(frl_list_entry_t);
const apr_uint32_t SIZEOF_FRL_LIST_T = sizeof(frl_list_t);
const apr_uint32_t SIZEOF_FRL_LIST_STAT_T = sizeof(frl_list_stat_t);

APR_DECLARE(frl_list_entry_t*) frl_list_get(frl_list_t* list, frl_key_t key);
APR_DECLARE(frl_list_entry_t*) frl_list_set(frl_list_t* list, frl_key_t key, void* pointer);
APR_DECLARE(frl_list_entry_t*) frl_list_add(frl_list_entry_t* entry, frl_insert_u insert, frl_key_t key, void* pointer);
APR_DECLARE(apr_status_t) frl_list_remove(frl_list_entry_t* elts);
APR_DECLARE(frl_list_stat_t) frl_list_stat(frl_list_t* list);
APR_DECLARE(void) frl_list_destroy(frl_list_t* list);
APR_DECLARE(apr_status_t) frl_list_create(frl_list_t** newlist, apr_pool_t* mempool, apr_uint32_t capacity, frl_lock_u lock, frl_slab_pool_t* pool = 0);

#endif
