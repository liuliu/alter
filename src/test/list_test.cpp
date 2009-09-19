#include "../include/frl_list.h"

apr_pool_t* global_pool;
frl_list_t* list;
apr_uint32_t uniqid = 0;

void* threadsafe_test(apr_thread_t* thd, void* data)
{
	apr_uint32_t id = (apr_uint32_t)(long long)data+1;
	apr_uint32_t size = (101-id)*100;
	apr_uint32_t* keys = (apr_uint32_t*)malloc(size*sizeof(apr_uint32_t));
	frl_list_entry_t** entries = (frl_list_entry_t**)malloc(size*sizeof(frl_list_entry_t**));
	printf("Thread %d: Adding List.\n", id);
	for (int i = 0; i < size; i++)
	{
		keys[i] = apr_atomic_inc32((volatile apr_uint32_t*)&uniqid);
		entries[i] = frl_list_add(list->tail, FRL_INSERT_BEFORE, keys[i], keys+i);
	}
	printf("Thread %d: Removing List.\n", id);
	for (int i = 0; i < size; i++)
		frl_list_remove(entries[i]);
	printf("Thread %d: Quit.\n", id);
	free(keys);
}

void* thread_watch(apr_thread_t* thd, void* data)
{
}

int main()
{
	apr_thread_t* thds[1000];
	apr_thread_t* thd_wch;
	apr_threadattr_t* thd_attr;
	apr_initialize();
	apr_pool_create(&global_pool, NULL);
	apr_atomic_init(global_pool);
	frl_list_create(&list, global_pool, 1, FRL_LOCK_FREE);
	apr_threadattr_create(&thd_attr, global_pool);
	//apr_thread_create(&thd_wch, thd_attr, threadsafe_watch, NULL, global_pool);
	apr_time_t now = apr_time_now();
	for (int i = 0; i < 100; i++)
		apr_thread_create(&thds[i], thd_attr, threadsafe_test, (void*)i, global_pool);
	apr_status_t rv;
	for (int i = 0; i < 100; i++)
	{
		printf("Stop at %d\n", i+1);
		apr_thread_join(&rv, thds[i]);
	}
	if (APR_SUCCESS == rv)
		printf("%d elements with %dus.\n", uniqid, apr_time_now()-now);
	frl_list_stat_t stat = frl_list_stat(list);
	printf("List Statement:\nBusy: %d\nTotal:%d\n", stat.busy, stat.nelts);
	apr_terminate();
	return 0;
}
