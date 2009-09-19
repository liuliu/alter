#include "../include/frl_slab_pool.h"

frl_slab_pool_t* pool;
apr_pool_t* global_pool;

void* threadsafe_test(apr_thread_t* thd, void* data)
{
	apr_uint32_t id = (apr_uint32_t)(long long)data+1;
	apr_uint32_t size = (101-id)*100;
	apr_uint32_t** pointer = (apr_uint32_t**)malloc(sizeof(apr_uint32_t**)*size);
	for (int i = 0; i < size; i++)
	{
		pointer[i] = (apr_uint32_t*)frl_slab_palloc(pool);
		*pointer[i] = i;
	}
	printf("Thread: %d, Allocated\n", id);
	for (int i = 0; i < size; i++)
	{
		if (*pointer[i] != i)
			printf("Thread: %d, Slot %d Unexpected Memory Overwrite with Value %d.\n", id, i, *pointer[i]);
		frl_slab_pfree(pointer[i]);
	}
	printf("Thread: %d, Released\n", id);
	free(pointer);
}

void* threadsafe_watch(apr_thread_t* thd, void* data)
{
	apr_status_t rv;
	do {
		rv = frl_slab_pool_safe(pool);
		if (FRL_MEMLEAK == rv)
			printf("Potential Memory Leak Detected.\n");
		else if (FRL_STACKBUSY == rv)
			printf("Memory Stack Busy Now.\n");
		else if (FRL_STACKERR == rv)
			printf("Memory Stack Error Detected.\n");
		apr_sleep(1000000);
	} while (1);
}

int main()
{
	apr_thread_t* thds[1000];
	apr_thread_t* thd_wch;
	apr_threadattr_t* thd_attr;
	apr_initialize();
	apr_pool_create(&global_pool, NULL);
	apr_atomic_init(global_pool);
	apr_threadattr_create(&thd_attr, global_pool);
	frl_slab_pool_create(&pool, global_pool, 100000, 4, FRL_LOCK_FREE);
	apr_thread_create(&thd_wch, thd_attr, threadsafe_watch, NULL, global_pool);
	apr_time_t now = apr_time_now();
	for (int i = 0; i < 100; i++)
		apr_thread_create(&thds[i], thd_attr, threadsafe_test, (void*)i, global_pool);
	apr_status_t rv;
	for (int i = 0; i < 100; i++)
	{
		printf("Stop at %d\n", i+1);
		apr_thread_join(&rv, thds[i]);
	}
	rv = frl_slab_pool_safe(pool);
	if (APR_SUCCESS == rv)
		printf("Pass with %dus.\n", apr_time_now()-now);
	else if (FRL_MEMLEAK == rv)
		printf("Potential Memory Leak Detected.\n");
	else if (FRL_STACKERR == rv)
		printf("Memory Stack Error Detected.\n");
	frl_mem_stat_t stat = frl_slab_pool_stat(pool);
	printf("Memory Statment:\n-per_size: %d\n-capacity: %d\n-usage: %d\n-block_size: %d\n", stat.per_size, stat.capacity, stat.usage, stat.block_size);
	apr_terminate();
}
