#include "../include/frl_queue.h"

apr_pool_t* global_pool;
frl_queue_t* queue;

void* threadsafe_test_push(apr_thread_t* thd, void* data)
{
	apr_uint32_t id = (apr_uint32_t)(long long)data+1;
	apr_uint32_t size = (101-id)*100;
	printf("Push Thread: %d, Allocated\n", id);
	for (int i = 0; i < size; i++)
	{
		frl_queue_push(queue, (void*)id);
	}
	printf("Push Thread: %d, Exited\n", id);
}

void* threadsafe_test_pop(apr_thread_t* thd, void* data)
{
	apr_uint32_t id = (apr_uint32_t)(long long)data+1;
	apr_uint32_t size = (101-id)*100;
	printf("Pop Thread: %d, Allocated\n", id);
	for (int i = 0; i < size; i++)
	{
		apr_uint32_t new_id = (apr_uint32_t)(apr_uint64_t)frl_queue_pop(queue);
		if ((new_id <= 0)||(new_id > 1000))
			printf("Error!");
	}
	printf("Pop Thread: %d, Exited\n", id);
}

int main()
{
	apr_thread_t* push_thds[1000];
	apr_thread_t* pop_thds[1000];
	apr_threadattr_t* thd_attr;
	apr_initialize();
	apr_pool_create(&global_pool, NULL);
	apr_atomic_init(global_pool);
	frl_queue_create(&queue, global_pool, 1, FRL_LOCK_FREE);
	apr_threadattr_create(&thd_attr, global_pool);
	apr_time_t now = apr_time_now();
	for (int i = 0; i < 100; i++)
	{
		apr_thread_create(&pop_thds[i], thd_attr, threadsafe_test_pop, (void*)i, global_pool);
		apr_thread_create(&push_thds[i], thd_attr, threadsafe_test_push, (void*)i, global_pool);
	}
	apr_status_t rv;
	for (int i = 0; i < 100; i++)
	{
		printf("Stop at %d\n", i+1);
		apr_thread_join(&rv, push_thds[i]);
		apr_thread_join(&rv, pop_thds[i]);
	}
	printf("Pass with %dus.\n", apr_time_now()-now);
	apr_terminate();
}
