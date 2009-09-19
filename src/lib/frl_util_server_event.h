/*************************
 * Fotas Runtime Library *
 * ***********************
 * socket server with event-driven function
 * author: Liu Liu
 */
#ifndef GUARD_frl_util_server_event_h
#define GUARD_frl_util_server_event_h

#include "frl_slab_pool.h"
#include "frl_logging.h"

#define SERVER_EVENT_POLL_TIMEOUT (1000000)

#define EVENT_STATE_TO_START(state) (state = state & 0x0)
#define EVENT_STATE_TO_COMPLETE(state) (state = state | 0x1)

#define FRL_EVENT_READ_START (0x02)
#define FRL_EVENT_READ_COMPLETE (0x03)
#define FRL_EVENT_SEND_START (0x04)
#define FRL_EVENT_SEND_COMPLETE (0x05)
#define FRL_EVENT_DISABLED (0x0)
#define FRL_EVENT_WAIT_RESPONSE (0x1)

class frl_server_event;

struct frl_notify_t
{
	apr_size_t size;
	char* buffer;
	apr_time_t timeout;
	int state;
};

struct frl_eventinfo_t
{
	frl_notify_t notify;
	char* reader;
	apr_off_t offset;
	apr_size_t size;
	int state;
};

struct frl_server_labor_t
{
	apr_uint32_t no;
	frl_server_event* master;
};

const apr_uint32_t SIZEOF_FRL_NOTIFY_T = sizeof(frl_notify_t);
const apr_uint32_t SIZEOF_FRL_EVENTINFO_T = sizeof(frl_eventinfo_t);
const apr_uint32_t SIZEOF_FRL_SERVER_LABOR_T = sizeof(frl_server_labor_t);

void* thread_server_event_listener(apr_thread_t* thd, void* data);
void* thread_server_event_handler(apr_thread_t* thd, void* data);

class frl_server_event
{
	friend void* thread_server_event_listener(apr_thread_t* thd, void* data);
	friend void* thread_server_event_handler(apr_thread_t* thd, void* data);
	private:
		frl_slab_pool_t* eventpool;

		apr_pool_t* mempool;
		apr_pool_t* sockpool;
		apr_thread_t* server_event_listen_thd;
		apr_thread_t** server_event_handler_thds;
		apr_threadattr_t* thd_attr;
		apr_pollset_t** pollset;

		apr_uint32_t* balance;
		apr_uint32_t min;
		apr_uint32_t max;
		apr_uint32_t open;
		apr_sockaddr_t* sock_addr;
		bool destroyed;

		frl_server_labor_t* labor;
		virtual apr_status_t recv_before(char** buf, apr_size_t* len)
		{
			return FRL_PROGRESS_CONTINUE;
		}
		virtual apr_status_t recv_send(char** buf, apr_size_t* len, int* state, apr_time_t* timeout)
		{
			return FRL_PROGRESS_COMLETE;
		}
		virtual apr_status_t send_after(char* buf, apr_size_t len)
		{
			return FRL_PROGRESS_COMLETE;
		}
	public:
		frl_server_event(apr_uint32_t _capacity, frl_lock_u lock, apr_pool_t* _mempool)
		: capacity(_capacity)
		{
			destroyed = 0;
			apr_pool_create(&mempool, _mempool);
			frl_slab_pool_create(&eventpool, mempool, capacity, SIZEOF_FRL_EVENTINFO_T, lock);
			apr_threadattr_create(&thd_attr, mempool);
			apr_pool_create(&sockpool, mempool);
		}
		virtual ~frl_server_event()
		{
			destroyed = 1;
			frl_slab_pool_destroy(eventpool);
			apr_pool_destroy(sockpool);
			apr_pool_destroy(mempool);
		}
		void spawn(apr_uint32_t _min, apr_uint32_t _max, apr_sockaddr_t* _sock_addr);
		void wait();
		apr_uint32_t capacity;
};

#endif
