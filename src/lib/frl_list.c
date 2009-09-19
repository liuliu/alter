/*************************
 * Fotas Runtime Library *
 *************************
 * Author: Liu Liu
 */
#include "frl_list.h"

APR_DECLARE(frl_list_entry_t*) frl_list_get_lock_free(frl_list_t* list, frl_key_t key)
{
}

#define DCAS_BUSY_VAL (0)
#define DCAS_EXIT_VAL (0)

APR_DECLARE(frl_list_entry_t*) frl_list_set_lock_free(frl_list_t* list, frl_key_t key, void* pointer)
{
}

APR_DECLARE(frl_list_entry_t*) frl_list_add_lock_free(frl_list_entry_t* entry, frl_insert_u insert, apr_uint32_t key, void* pointer)
{
}

APR_DECLARE(apr_status_t) frl_list_remove_lock_free(frl_list_entry_t* elts)
{
}

APR_DECLARE(frl_list_entry_t*) frl_list_get_lock_with(frl_list_t* list, frl_key_t key)
{
#if APR_HAS_THREADS
	apr_thread_rwlock_rdlock(list->rwlock);
#endif
	frl_list_entry_t* elts = list->head->next;
	while ((elts != list->tail)&&(elts != 0))
	{
		if (elts->key == key)
		{
#if APR_HAS_THREADS
			apr_thread_rwlock_unlock(list->rwlock);
#endif
			return elts;
		}
		elts = elts->next;
	}
#if APR_HAS_THREADS
	apr_thread_rwlock_unlock(list->rwlock);
#endif
	return 0;
}

APR_DECLARE(frl_list_entry_t*) frl_list_set_lock_with(frl_list_t* list, frl_key_t key, void* pointer)
{
	int i;
#if APR_HAS_THREADS
	apr_thread_rwlock_wrlock(list->rwlock);
#endif
	frl_list_entry_t* elts = list->head;
	while ((elts != list->tail)&&(elts != 0))
	{
		if (elts->key == key)
		{
			elts->pointer = pointer;
#if APR_HAS_THREADS
			apr_thread_rwlock_unlock(list->rwlock);
#endif
			return elts;
		}
		elts = elts->next;
	}
#if APR_HAS_THREADS
	apr_thread_rwlock_unlock(list->rwlock);
#endif
	return 0;
}

APR_DECLARE(frl_list_entry_t*) frl_list_add_lock_with(frl_list_entry_t* entry, frl_insert_u insert, frl_key_t key, void* pointer)
{
	frl_list_t* list = entry->list;
	frl_list_entry_t* elts = (frl_list_entry_t*)frl_slab_palloc(list->pool);
	if (elts != 0)
	{
		elts->key = key;
		elts->pointer = pointer;
		elts->list = list;
#if APR_HAS_THREADS
		apr_thread_rwlock_wrlock(list->rwlock);
#endif
		if (FRL_INSERT_AFTER == insert)
		{
			elts->next = entry->next;
			elts->prev = entry;
			entry->next = elts;
		}
		if (FRL_INSERT_BEFORE == insert)
		{
			elts->next = entry;
			elts->prev = entry->prev;
			entry->prev = elts;
		}
		apr_atomic_inc32(&list->nelts);
#if APR_HAS_THREADS
		apr_thread_rwlock_unlock(list->rwlock);
#endif
		return elts;
	}
	return 0;
}

APR_DECLARE(apr_status_t) frl_list_remove_lock_with(frl_list_entry_t* elts)
{
	frl_list_t* list = elts->list;
#if APR_HAS_THREADS
	apr_thread_rwlock_wrlock(list->rwlock);
#endif
	if (elts->prev != 0)
		elts->prev->next = elts->next;
	if (elts->next != 0)
		elts->next->prev = elts->prev;
	apr_atomic_dec32(&list->nelts);
#if APR_HAS_THREADS
	apr_thread_rwlock_unlock(list->rwlock);
#endif
	frl_slab_pfree(elts);
	return APR_SUCCESS;
}

APR_DECLARE(frl_list_entry_t*) frl_list_get(frl_list_t* list, frl_key_t key)
{
	if (FRL_LOCK_FREE == list->lock)
		return frl_list_get_lock_free(list, key);
	else if (FRL_LOCK_WITH == list->lock)
		return frl_list_get_lock_with(list, key);
}

APR_DECLARE(frl_list_entry_t*) frl_list_set(frl_list_t* list, frl_key_t key, void* pointer)
{
	if (FRL_LOCK_FREE == list->lock)
		return frl_list_set_lock_free(list, key, pointer);
	else if (FRL_LOCK_WITH == list->lock)
		return frl_list_set_lock_with(list, key, pointer);
}

APR_DECLARE(frl_list_entry_t*) frl_list_add(frl_list_entry_t* entry, frl_insert_u insert, frl_key_t key, void* pointer)
{
	frl_list_t* list = entry->list;
	if (FRL_LOCK_FREE == list->lock)
		return frl_list_add_lock_free(entry, insert, key, pointer);
	else if (FRL_LOCK_WITH == list->lock)
		return frl_list_add_lock_with(entry, insert, key, pointer);
}

APR_DECLARE(apr_status_t) frl_list_remove(frl_list_entry_t* elts)
{
	frl_list_t* list = elts->list;
	if (FRL_LOCK_FREE == list->lock)
		return frl_list_remove_lock_free(elts);
	else if (FRL_LOCK_WITH == list->lock)
		return frl_list_remove_lock_with(elts);
}

APR_DECLARE(frl_list_stat_t) frl_list_stat(frl_list_t* list)
{
	frl_list_stat_t stat;
	stat.nelts = 0;
	stat.busy = 0;
	frl_list_entry_t* elts = list->head->next;
	while (elts != list->tail)
	{
		stat.nelts++;
		if (DCAS_EXIT_VAL == elts->pointer)
			stat.busy++;
		elts = elts->next;
	}
	return stat;
}

APR_DECLARE(void) frl_list_destroy(frl_list_t* list)
{
#if APR_HAS_THREADS
	apr_thread_rwlock_destroy(list->rwlock);
#endif
	frl_list_entry_t* elts = list->head;
	frl_list_entry_t* last = 0;
	while (elts != 0)
	{
		last = elts;
		elts = elts->next;
		frl_slab_pfree(last);
	}
	if (FRL_MEMORY_SELF == list->memory)
		frl_slab_pool_destroy(list->pool);
	free(list);
}

APR_DECLARE(apr_status_t) frl_list_create(frl_list_t** newlist, apr_pool_t* mempool, apr_uint32_t capacity, frl_lock_u lock, frl_slab_pool_t* pool)
{
	frl_list_t *list;
	list = (frl_list_t*)malloc(SIZEOF_FRL_LIST_T);
	if (list == 0)
		return APR_ENOMEM;

	if (pool == 0)
	{
		frl_slab_pool_create(&list->pool, mempool, capacity, SIZEOF_FRL_LIST_ENTRY_T, lock);
		list->memory = FRL_MEMORY_SELF;
	} else {
		list->pool = pool;
		list->memory = FRL_MEMORY_GLOBAL;
	}

#if APR_HAS_THREADS
	apr_thread_rwlock_create(&list->rwlock, mempool);
#endif

	list->head = (frl_list_entry_t*)frl_slab_palloc(list->pool);
	list->tail = (frl_list_entry_t*)frl_slab_palloc(list->pool);
	list->head->prev = 0;
	list->head->next = list->tail;
	list->head->list = list;
	list->tail->prev = list->head;
	list->tail->next = 0;
	list->tail->list = list;
	list->nelts = 0;
	list->lock = FRL_LOCK_WITH;
	*newlist = list;
	return APR_SUCCESS;
}
