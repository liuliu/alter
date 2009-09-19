/*************************
 * Fotas Runtime Library *
 * ***********************
 * leader-follower thread pool model
 * author: Liu Liu
 */
#include "frl_util_threads.h"

void* thread_leader(apr_thread_t* thd, void* data)
{
	frl_thread_config_t* config = (frl_thread_config_t*)data;
	apr_uint32_t no = config->no;
	frl_threads* master = config->master;
	frl_thread_select_t* leader = master->leader;
	do {
		//become the leader
		apr_thread_mutex_lock(leader->mutex);
		while (leader->no >= 0)
			apr_thread_cond_wait(leader->select, leader->mutex);
		leader->no = no;
		apr_thread_mutex_unlock(leader->mutex);

		//wait message
		void* pointer = frl_queue_pop(master->task_queue);

		//select new leader
		leader->no = -1;
		apr_thread_cond_signal(leader->select);

		//assign a task
		apr_atomic_dec32(&master->waiters);

		//process message
		if ((pointer == 0)||(APR_SUCCESS != master->execute(pointer)))
			master->thd_stat[no].failed++;
		master->thd_stat[no].handled++;

		apr_atomic_inc32(&master->waiters);
	} while (!master->destroyed);
}

void* thread_consumer(apr_thread_t* thd, void* data)
{
	frl_thread_config_t* config = (frl_thread_config_t*)data;
	apr_uint32_t no = config->no;
	frl_threads* master = config->master;
	frl_thread_select_t* consumer = master->consumers+no;
	do {
		apr_thread_mutex_lock(consumer->mutex);
		apr_thread_cond_wait(consumer->select, consumer->mutex);
		apr_thread_mutex_unlock(consumer->mutex);
		if ((consumer->pointer == 0)||(APR_SUCCESS != master->execute(consumer->pointer)))
			master->thd_stat[no].failed++;
		master->thd_stat[no].handled++;
		apr_queue_push(master->consumer_queue, consumer);
	} while (!master->destroyed);
}

void* thread_producer(apr_thread_t* thd, void* data)
{
	frl_threads* master = (frl_threads*)data;
	do {
		void* pointer = frl_queue_pop(master->task_queue);
		if (pointer > 0)
		{
			frl_thread_select_t* consumer;
			if (APR_SUCCESS == apr_queue_pop(master->consumer_queue, (void**)&consumer))
			{
				consumer->pointer = pointer;
				apr_thread_cond_signal(consumer->select);
			}
		}
	} while (!master->destroyed);
}

apr_status_t frl_threads::spawn(apr_uint32_t min, apr_uint32_t max)
{
	this->min = min;
	this->max = max;
	thd_worker = (apr_thread_t**)apr_palloc(mempool, max*SIZEOF_POINTER);
	thd_stat = (frl_thread_stat_t*)apr_palloc(mempool, max*SIZEOF_FRL_THREAD_STAT_T);
	thd_config = (frl_thread_config_t*)apr_palloc(mempool, max*SIZEOF_FRL_THREAD_CONFIG_T);
	frl_queue_create(&task_queue, mempool, max, lock);
	apr_thread_mutex_create(&mutex, APR_THREAD_MUTEX_UNNESTED, mempool);
	apr_thread_cond_create(&select, mempool);
	if (FRL_THREAD_LEADER_FOLLOWER == model)
	{
		leader = (frl_thread_select_t*)apr_palloc(mempool, SIZEOF_FRL_THREAD_SELECT_T);
		apr_thread_mutex_create(&leader->mutex, APR_THREAD_MUTEX_UNNESTED, mempool);
		apr_thread_cond_create(&leader->select, mempool);
		for (int i = 0; i < max; i++)
		{
			thd_config[i].no = i;
			thd_config[i].master = this;
		}
		open = waiters = min;
		leader->no = -1;
		for (int i = 0; i < min; i++)
			apr_thread_create(thd_worker+i, thd_attr, thread_leader, thd_config+i, mempool);
	} else if (FRL_THREAD_CONSUMER_PRODUCER == model) {
		consumers = (frl_thread_select_t*)apr_palloc(mempool, max*SIZEOF_FRL_THREAD_SELECT_T);
		apr_queue_create(&consumer_queue, max, mempool);
		for (int i = 0; i < max; i++)
		{
			thd_config[i].no = i;
			thd_config[i].master = this;
			consumers[i].pointer = 0;
			apr_thread_mutex_create(&consumers[i].mutex, APR_THREAD_MUTEX_UNNESTED, mempool);
			apr_thread_cond_create(&consumers[i].select, mempool);
		}
		open = waiters = min;
		for (int i = 0; i < min; i++)
		{
			apr_thread_create(thd_worker+i, thd_attr, thread_consumer, thd_config+i, mempool);
			apr_queue_push(consumer_queue, consumers+i);
		}
		apr_thread_create(&thd_master, thd_attr, thread_producer, this, mempool);
	}
	return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) frl_threads::assign(void* pointer)
{
	frl_queue_push(task_queue, pointer);
	if ((waiters <= 0)&&(open < max))
	{
		apr_thread_mutex_lock(this->mutex);
		if ((waiters <= 0)&&(open < max))
		{
			if (FRL_THREAD_LEADER_FOLLOWER == model)
			{
				apr_thread_create(thd_worker+open, thd_attr, thread_leader, thd_config+open, mempool);
			} else if (FRL_THREAD_CONSUMER_PRODUCER == model) {
				apr_thread_create(thd_worker+open, thd_attr, thread_consumer, thd_config+open, mempool);
				apr_queue_push(consumer_queue, consumers+open);
			}
			open++;
		}
		apr_thread_mutex_unlock(this->mutex);
	}
	return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) frl_threads::wait()
{
	apr_status_t status;
	if (FRL_THREAD_LEADER_FOLLOWER == model)
	{
		for (int i = 0; i < open; i++)
			apr_thread_join(&status, thd_worker[i]);
		return APR_SUCCESS;
	} else if (FRL_THREAD_CONSUMER_PRODUCER == model) {
		apr_thread_join(&status, thd_master);
		for (int i = 0; i < open; i++)
			apr_thread_join(&status, thd_worker[i]);
		return APR_SUCCESS;
	}
}

APR_DECLARE(frl_thread_stat_t) frl_threads::state(apr_uint32_t no)
{
	return thd_stat[no];
}
