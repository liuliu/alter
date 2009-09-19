#include "apr_hash.h"
#include "apr_tables.h"
#include "../include/frl_util_md5.h"
#include "../include/frl_radix_tree.h"
#include <map>

apr_pool_t* mempool;
frl_radix_tree_t* tree;

void* threadsafe_test(apr_thread_t* thd, void* data)
{
}

int main()
{
	apr_initialize();
	apr_pool_create(&mempool, NULL);
	apr_atomic_init(mempool);

	frl_radix_tree_create(&tree, mempool, 16, 1000, FRL_LOCK_FREE);
	frl_md5 key_cache[500000];
	for (int i = 0; i < 500000; i++)
		key_cache[i] = frl_md5((char*)&i, 4);
	apr_time_t time = apr_time_now();
	int a[] = {10, 11, 12, 13, 22};

	for (int i = 0; i < 500000; i++)
		frl_radix_tree_add(tree, key_cache[i].digest, a+i%5);
	time = apr_time_now()-time;
	printf("added 500000 key-value pairs to radix tree in %d microsecond.\n", time);
	time = apr_time_now();
	for (int i = 0; i < 500000; i++)
	{
		frl_radix_tree_entry_t* entry = frl_radix_tree_get(tree, key_cache[i].digest);
		if (entry->pointer != a+i%5)
			printf("unexpected error in looking up.\n");
	}
	time = apr_time_now()-time;
	printf("looked up 500000 key-value pairs in radix tree in %d microsecond.\n", time);
	time = apr_time_now();
	for (int i = 0; i < 500000; i++)
	{
		frl_radix_tree_entry_t* entry = frl_radix_tree_get(tree, key_cache[i].digest);
		if (entry->pointer != a+i%5)
			printf("unexpected error in looking up.\n");
		apr_status_t status = frl_radix_tree_remove(entry);
		if (status != APR_SUCCESS)
			printf("unexpected error in removing.\n");
	}
	time = apr_time_now()-time;
	printf("removed 500000 key-value pairs to radix tree in %d microsecond.\n", time);

	apr_hash_t* apr_hash = apr_hash_make(mempool);
	time = apr_time_now();
	for (int i = 0; i < 500000; i++)
		apr_hash_set(apr_hash, key_cache[i].digest, 16, a+i%5);
	time = apr_time_now()-time;
	printf("added 500000 key-value pairs to apr hash table in %d microsecond.\n", time);
	time = apr_time_now();
	for (int i = 0; i < 500000; i++)
	{
		void* entry = apr_hash_get(apr_hash, key_cache[i].digest, 16);
		if (entry != a+i%5)
			printf("unexpected error in looking up.\n");
	}
	time = apr_time_now()-time;
	printf("looked up 500000 key-value pairs in apr hash table in %d microsecond.\n", time);

	/*
	apr_table_t* apr_table = apr_table_make(mempool, 500000);
	time = apr_time_now();
	char table_data[] = "oh my god!\0";
	for (int i = 0; i < 500000; i++)
	{
		apr_byte_t q[23];
		key_cache[i].base64_encode(q);
		apr_table_set(apr_table, (char*)q, table_data);
	}
	time = apr_time_now()-time;
	printf("added 500000 key-value pairs to apr table in %d microsecond.\n", time);
	time = apr_time_now();
	for (int i = 0; i < 500000; i++)
	{
		apr_byte_t q[23];
		key_cache[i].base64_encode(q);
		const void* entry = apr_table_get(apr_table, (char*)q);
	}
	time = apr_time_now()-time;
	printf("looked up 500000 key-value pairs in apr table in %d microsecond.\n", time);
	*/

	std::map<frl_md5, int> map;
	time = apr_time_now();
	for (int i = 0; i < 500000; i++)
		map[key_cache[i]] = a[i%5];
	time = apr_time_now()-time;
	printf("added 500000 key-value pairs to map in %d microsecond\n", time);
	time = apr_time_now();
	for (int i = 0; i < 500000; i++)
	{
		int n = map[key_cache[i]];
		if (n != a[i%5])
			printf("unexpected error in looking up.\n");
	}
	time = apr_time_now()-time;
	printf("looked up 500000 key-value pairs in map in %d microsecond\n", time);
	time = apr_time_now();
	for (int i = 0; i < 500000; i++)
		map.erase(key_cache[i]);
	time = apr_time_now()-time;
	printf("looked up 500000 key-value pairs in map in %d microsecond\n", time);
	
	apr_terminate();
	return 0;
}
