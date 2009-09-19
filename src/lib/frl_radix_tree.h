#ifndef GUARD_frl_radix_tree_h
#define GUARD_frl_radix_tree_h

#include "frl_slab_pool.h"

struct frl_radix_tree_t;

struct frl_radix_tree_entry_t
{
	apr_uint32_t depth;
	apr_uint32_t nelts;
	void* pointer;
	apr_byte_t key[32];
	frl_radix_tree_t* tree;
	frl_radix_tree_entry_t* parent;
	frl_radix_tree_entry_t* children[256];
};

struct frl_radix_tree_t
{
	frl_slab_pool_t* pool;
	apr_uint32_t nelts;
	frl_radix_tree_entry_t* root;
	frl_lock_u lock;
	frl_memory_u memory;
	apr_uint32_t key_size;
#if APR_HAS_THREADS
	apr_thread_rwlock_t* rwlock;
#endif
};

const apr_uint32_t SIZEOF_FRL_RADIX_TREE_T = sizeof(frl_radix_tree_t);
const apr_uint32_t SIZEOF_FRL_RADIX_TREE_ENTRY_T = sizeof(frl_radix_tree_entry_t);

APR_DECLARE(frl_radix_tree_entry_t*) frl_radix_tree_get(frl_radix_tree_t* tree, apr_byte_t* key);
APR_DECLARE(frl_radix_tree_entry_t*) frl_radix_tree_add(frl_radix_tree_t* tree, apr_byte_t* key, void* pointer);
APR_DECLARE(apr_status_t) frl_radix_tree_remove(frl_radix_tree_entry_t* elts);
APR_DECLARE(void*) frl_radix_tree_destroy(frl_radix_tree_t* tree);
APR_DECLARE(apr_status_t) frl_radix_tree_create(frl_radix_tree_t** newtree, apr_pool_t* mempool, apr_uint32_t key_size, apr_size_t capacity, frl_lock_u lock, frl_slab_pool_t* pool = 0);

#endif
