#include "../include/frl_util_threads.h"
#include <iostream>

apr_pool_t* global_pool;

class TestCase : public frl_threads
{
	private:
		virtual apr_status_t execute(void* pointer)
		{
			int n = rand()%2000000;
			printf("Start Task No.%d\n", (int)(long long)pointer);
			apr_sleep(n);
			printf("Exit Task No.%d\n", (int)(long long)pointer);
			return APR_SUCCESS;
		}
	public:
		TestCase(frl_thread_model_u _model, frl_lock_u _lock, apr_pool_t* _mempool)
		: frl_threads(_model, _lock, _mempool) {}
};

int main()
{
	apr_initialize();
	apr_pool_create(&global_pool, NULL);
	apr_atomic_init(global_pool);
	TestCase* drive1 = new TestCase(FRL_THREAD_CONSUMER_PRODUCER, FRL_LOCK_FREE, global_pool);
	drive1->spawn(10, 10);
	for (int i = 0; i < 100; i++)
		drive1->assign((void*)i);
	drive1->wait();
	apr_terminate();
}
