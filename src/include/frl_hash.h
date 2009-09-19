#ifndef GUARD_frl_hash_h
#define GUARD_frl_hash_h

#include "frl_list.h"

struct frl_hash_t
{
	apr_uint32_t nelts;
	frl_list_t** entry;
	frl_slab_pool_t* pool;
	frl_memory_u memory;
};

const apr_uint32_t SIZEOF_FRL_HASH_T = sizeof(frl_hash_t);

APR_DECLARE(void*) frl_hash_get(frl_hash_t* hash, frl_key_t key);
APR_DECLARE(apr_status_t) frl_hash_set(frl_hash_t* hash, frl_key_t key, void* pointer);
APR_DECLARE(apr_status_t) frl_hash_add(frl_hash_t* hash, frl_key_t key, void* pointer);
APR_DECLARE(apr_status_t) frl_hash_remove(frl_hash_t* hash, frl_key_t key);
APR_DECLARE(void*) frl_hash_destroy(frl_hash_t* hash);
APR_DECLARE(apr_status_t) frl_hash_create(frl_hash_t** newhash, apr_pool_t* mempool, apr_size_t capacity, frl_lock_u lock, frl_slab_pool_t* pool = 0);

#endif
