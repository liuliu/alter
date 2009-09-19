/*************************
 * Fotas Runtime Library *
 *************************
 * Author: Liu Liu
 */

#include "frl_hash.h"

//a very simple hash function
APR_DECLARE(apr_uint32_t) frl_hash_func(frl_key_t key, apr_uint32_t nelts)
{
	return (key*33)%701%nelts;
}

APR_DECLARE(void*) frl_hash_get(frl_hash_t* hash, frl_key_t key)
{
	frl_list_entry_t* entry = frl_list_get(hash->entry[frl_hash_func(key, hash->nelts)], key);
	if (entry == NULL)
		return NULL;
	return entry->pointer;
}

APR_DECLARE(apr_status_t) frl_hash_set(frl_hash_t* hash, frl_key_t key, void* pointer)
{
	if (frl_list_set(hash->entry[frl_hash_func(key, hash->nelts)], key, pointer) != NULL)
		return APR_SUCCESS;
	else
		return APR_BADARG;
}

APR_DECLARE(apr_status_t) frl_hash_add(frl_hash_t* hash, frl_key_t key, void* pointer)
{
	if (frl_list_add(hash->entry[frl_hash_func(key, hash->nelts)]->tail, FRL_INSERT_BEFORE, key, pointer) != NULL)
		return APR_SUCCESS;
	else
		return APR_BADARG;
}

APR_DECLARE(apr_status_t) frl_hash_remove(frl_hash_t* hash, frl_key_t key)
{
	frl_list_entry_t* entry = frl_list_get(hash->entry[frl_hash_func(key, hash->nelts)], key);
	if (entry == NULL)
		return APR_BADARG;
	return frl_list_remove( entry);
}

APR_DECLARE(void*) frl_hash_destroy(frl_hash_t* hash)
{
	int i;
	for (i = 0; i < hash->nelts; i++)
	{
		frl_list_destroy(hash->entry[i]);
	}
	if (FRL_MEMORY_SELF == hash->memory)
		frl_slab_pool_destroy(hash->pool);
}

APR_DECLARE(apr_status_t) frl_hash_create(frl_hash_t** newhash, apr_pool_t* mempool, apr_size_t capacity, frl_lock_u lock, frl_slab_pool_t* pool = 0)
{
	int i;
	frl_hash_t* hash = (frl_hash_t*)apr_palloc(mempool, SIZEOF_FRL_HASH_T);
	if (hash == NULL)
		return APR_ENOMEM;
	hash->nelts = capacity;
	hash->entry = (frl_list_t**)apr_palloc(mempool, capacity*sizeof(frl_list_t**));
	if (hash->entry == NULL)
		return APR_ENOMEM;

	if (pool == 0)
	{
		frl_slab_pool_create(&hash->pool, mempool, capacity, SIZEOF_FRL_LIST_ENTRY_T, look);
		hash->memory = FRL_MEMORY_SELF;
	} else {
		hash->pool = pool;
		hash->memory = FRL_MEMORY_GLOBAL;
	}

	for (i = 0; i < hash->nelts; i++)
	{
		apr_status_t rv = frl_list_create(&hash->entry[i], mempool, hash->key_size, capacity, lock, hash->pool);
		if (rv != APR_SUCCESS)
			return rv;
	}
	*newhash = hash;
	return APR_SUCCESS;
}
