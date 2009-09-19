#include "frl_queue.h"

APR_DECLARE(apr_status_t) frl_queue_push_lock_free(frl_queue_t* queue, void* pointer)
{
	frl_queue_entry_t* elts = (frl_queue_entry_t*)frl_slab_palloc(queue->pool);
	if (elts == 0)
		return APR_ENOMEM;
	elts->next = 0;
	elts->pointer = pointer;
	frl_queue_entry_t* tail = 0;
	void* swap = 0;
	for (;;)
	{
		tail = queue->tail;
		swap = apr_atomic_casptr((volatile void**)&tail->next, elts, 0);
		if (swap == 0)
			break;
		apr_atomic_casptr((volatile void**)&queue->tail, swap, tail);
	}
	apr_atomic_casptr((volatile void**)&queue->tail, elts, tail);
	apr_atomic_inc32(&queue->nelts);
#if APR_HAS_THREADS
	if (queue->empty_waiters)
		apr_thread_cond_signal(queue->not_empty);
#endif
	return APR_SUCCESS;
}

APR_DECLARE(void*) frl_queue_pop_lock_free(frl_queue_t* queue)
{
	void* pointer = 0;
	frl_queue_entry_t* head = 0;
	frl_queue_entry_t* tail = 0;
	frl_queue_entry_t* next = 0;
	frl_queue_entry_t* swap = 0;
	for (;;)
	{
		head = queue->head;
		tail = queue->tail;
		next = head->next;
		if (head == tail)
		{
			if (next == 0)
			{
#if APR_HAS_THREADS
				apr_atomic_inc32(&queue->empty_waiters);
				apr_thread_mutex_lock(queue->mutex);
				apr_thread_cond_wait(queue->not_empty, queue->mutex);
				apr_thread_mutex_unlock(queue->mutex);
				apr_atomic_dec32(&queue->empty_waiters);
				if (queue->destroyed)
					return 0;
#else
				return 0;
#endif
			} else
				apr_atomic_casptr((volatile void**)&queue->tail, next, tail);
		} else if (next != 0) {
			swap = (frl_queue_entry_t*)apr_atomic_casptr((volatile void**)&queue->head, next, head);
			if (swap == head)
			{
				pointer = next->pointer;
				frl_slab_pfree(head);
				apr_atomic_dec32(&queue->nelts);
				return pointer;
			}
		}
	}
}

APR_DECLARE(apr_status_t) frl_queue_push_lock_with(frl_queue_t* queue, void* pointer)
{
#if APR_HAS_THREADS
	apr_thread_mutex_lock(queue->mutex);
#endif
	frl_queue_entry_t* elts = (frl_queue_entry_t*)frl_slab_palloc(queue->pool);
	if (elts == 0)
	{
#if APR_HAS_THREADS
		apr_thread_mutex_unlock(queue->mutex);
#endif
		return APR_ENOMEM;
	}
	elts->next = 0;
	elts->pointer = pointer;
	queue->tail->next = elts;
	queue->tail = elts;
	queue->nelts++;
	if (queue->empty_waiters)
		apr_thread_cond_signal(queue->not_empty);
#if APR_HAS_THREADS
	apr_thread_mutex_unlock(queue->mutex);
#endif
	return APR_SUCCESS;
}

APR_DECLARE(void*) frl_queue_pop_lock_with(frl_queue_t* queue)
{
#if APR_HAS_THREADS
	apr_thread_mutex_lock(queue->mutex);
#endif
	void* pointer = 0;
	for (;;)
	{
		if (queue->head == queue->tail)
		{
#if APR_HAS_THREADS
			queue->empty_waiters++;
			apr_thread_cond_wait(queue->not_empty, queue->mutex);
			queue->empty_waiters--;
			if (queue->destroyed)
				return 0;
#else
			return 0;
#endif
		} else {
			frl_queue_entry_t* head = queue->head;
			pointer = head->next->pointer;
			queue->head = head->next;
			frl_slab_pfree(head);
			queue->nelts--;
			break;
		}
	}
#if APR_HAS_THREADS
	apr_thread_mutex_unlock(queue->mutex);
#endif
	return pointer;
}

APR_DECLARE(apr_status_t) frl_queue_trypush_lock_free(frl_queue_t* queue, void* pointer)
{
	frl_queue_entry_t* elts = (frl_queue_entry_t*)frl_slab_palloc(queue->pool);
	if (elts == 0)
		return APR_ENOMEM;
	elts->next = 0;
	elts->pointer = pointer;
	frl_queue_entry_t* tail = 0;
	frl_queue_entry_t* swap = 0;
	for (;;)
	{
		tail = queue->tail;
		swap = (frl_queue_entry_t*)apr_atomic_casptr((volatile void**)&tail->next, elts, 0);
		if (swap == 0)
			break;
		apr_atomic_casptr((volatile void**)&queue->tail, swap, tail);
	}
	apr_atomic_casptr((volatile void**)&queue->tail, elts, tail);
	apr_atomic_inc32(&queue->nelts);
	return APR_SUCCESS;
}

APR_DECLARE(void*) frl_queue_trypop_lock_free(frl_queue_t* queue)
{
	void* pointer = 0;
	frl_queue_entry_t* head = 0;
	frl_queue_entry_t* tail = 0;
	frl_queue_entry_t* next = 0;
	frl_queue_entry_t* swap = 0;
	for (;;)
	{
		head = queue->head;
		tail = queue->tail;
		next = head->next;
		if (head == tail)
		{
			if (next == 0)
				return 0;
			apr_atomic_casptr((volatile void**)&queue->tail, next, tail);
		} else {
			swap = (frl_queue_entry_t*)apr_atomic_casptr((volatile void**)&queue->head, next, head);
			if (swap == head)
			{
				pointer = next->pointer;
				frl_slab_pfree(head);
				apr_atomic_dec32(&queue->nelts);
				return pointer;
			}
		}
	}
}

APR_DECLARE(apr_status_t) frl_queue_trypush_lock_with(frl_queue_t* queue, void* pointer)
{
#if APR_HAS_THREADS
	apr_thread_mutex_lock(queue->mutex);
#endif
	frl_queue_entry_t* elts = (frl_queue_entry_t*)frl_slab_palloc(queue->pool);
	if (elts == 0)
	{
#if APR_HAS_THREADS
		apr_thread_mutex_unlock(queue->mutex);
#endif
		return APR_ENOMEM;
	}
	elts->next = 0;
	elts->pointer = pointer;
	queue->tail->next = elts;
	queue->tail = elts;
	queue->nelts++;
#if APR_HAS_THREADS
	apr_thread_mutex_unlock(queue->mutex);
#endif
	return APR_SUCCESS;
}

APR_DECLARE(void*) frl_queue_trypop_lock_with(frl_queue_t* queue)
{
#if APR_HAS_THREADS
	apr_thread_mutex_lock(queue->mutex);
#endif
	void* pointer = 0;
	if (queue->head != queue->tail)
	{
		frl_queue_entry_t* head = queue->head;
		pointer = head->next->pointer;
		queue->head = head->next;
		frl_slab_pfree(head);
		queue->nelts--;
	}
#if APR_HAS_THREADS
	apr_thread_mutex_unlock(queue->mutex);
#endif
	return pointer;
}

APR_DECLARE(apr_status_t) frl_queue_push(frl_queue_t* queue, void* pointer)
{
	if (FRL_LOCK_FREE == queue->lock)
		return frl_queue_push_lock_free(queue, pointer);
	else if (FRL_LOCK_WITH == queue->lock)
		return frl_queue_push_lock_with(queue, pointer);
}

APR_DECLARE(void*) frl_queue_pop(frl_queue_t* queue)
{
	if (FRL_LOCK_FREE == queue->lock)
		return frl_queue_pop_lock_free(queue);
	else if (FRL_LOCK_WITH == queue->lock)
		return frl_queue_pop_lock_with(queue);
}

APR_DECLARE(apr_status_t) frl_queue_trypush(frl_queue_t* queue, void* pointer)
{
	if (FRL_LOCK_FREE == queue->lock)
		return frl_queue_trypush_lock_free(queue, pointer);
	else if (FRL_LOCK_WITH == queue->lock)
		return frl_queue_trypush_lock_with(queue, pointer);
}

APR_DECLARE(void*) frl_queue_trypop(frl_queue_t* queue)
{
	if (FRL_LOCK_FREE == queue->lock)
		return frl_queue_trypop_lock_free(queue);
	else if (FRL_LOCK_WITH == queue->lock)
		return frl_queue_trypop_lock_with(queue);
}

APR_DECLARE(void*) frl_queue_peek(frl_queue_t* queue)
{
	frl_queue_entry_t* head = queue->head;
	frl_queue_entry_t* tail = queue->tail;
	if (head == tail)
		return 0;
	else
		return head->pointer;
}

APR_DECLARE(void) frl_queue_destroy(frl_queue_t* queue)
{
#if APR_HAS_THREADS
	queue->destroyed = 1;
	apr_thread_cond_broadcast(queue->not_empty);
	apr_thread_cond_destroy(queue->not_empty);
	apr_thread_mutex_destroy(queue->mutex);
#endif
	if (FRL_MEMORY_SELF == queue->memory)
		frl_slab_pool_destroy(queue->pool);

	free(queue);
}

APR_DECLARE(apr_status_t) frl_queue_create(frl_queue_t** newqueue, apr_pool_t* mempool, apr_uint32_t capacity, frl_lock_u lock, frl_slab_pool_t* pool)
{
	frl_queue_t* queue;
	queue = (frl_queue_t*)malloc(SIZEOF_FRL_QUEUE_T);
	if (queue == 0)
		return APR_ENOMEM;

	if (pool == 0)
	{
		frl_slab_pool_create(&queue->pool, mempool, capacity, SIZEOF_FRL_QUEUE_ENTRY_T, lock);
		queue->memory = FRL_MEMORY_SELF;
	} else {
		queue->pool = pool;
		queue->memory = FRL_MEMORY_GLOBAL;
	}
	
	queue->lock = lock;
	queue->head = (frl_queue_entry_t*)frl_slab_palloc(queue->pool);
	queue->tail = queue->head;
	queue->tail->next = 0;
	queue->nelts = 0;
	queue->empty_waiters = 0;
	queue->destroyed = 0;

#if APR_HAS_THREADS
	apr_thread_cond_create(&queue->not_empty, mempool);
	apr_thread_mutex_create(&queue->mutex, APR_THREAD_MUTEX_UNNESTED, mempool);
#endif
	
	*newqueue = queue;
	return APR_SUCCESS;
}
