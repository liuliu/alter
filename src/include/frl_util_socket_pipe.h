/*************************
 * Fotas Runtime Library *
 * ***********************
 * socket pipe with replications
 * author: Liu Liu
 */
#ifndef GUARD_frl_util_socket_pipe_h
#define GUARD_frl_util_socket_pipe_h

#include "frl_slab_pool.h"
#include "frl_queue.h"
#include "frl_hash_func.h"
#include "frl_logging.h"

#define SOCKET_PIPE_POLL_TIMEOUT (1000000)
#define SOCKET_PACKAGE_SIZE (4092)
#define SOCKET_PIPE_SENDER (0x1)
#define SOCKET_PIPE_RECEIVER (0x2)

struct frl_pipe_header_t
{
	apr_uint32_t size;
	apr_uint32_t hash;
};

struct frl_pipe_block_t
{
	frl_pipe_header_t header;
	char start;
};

struct frl_pipe_data_t
{
	char* buf;
	union {
		apr_uint32_t hash;
		apr_off_t offset;
	};
	apr_size_t size;
};

#define PIPE_STATE_TO_START(state) (state = state & 0x0)
#define PIPE_STATE_TO_COMPLETE(state) (state = state | 0x1)

#define FRL_PIPE_READ_HEADER_START (0x02)
#define FRL_PIPE_READ_HEADER_COMPLETE (0x03)
#define FRL_PIPE_SEND_HEADER_START (0x04)
#define FRL_PIPE_SEND_HEADER_COMPLETE (0x05)
#define FRL_PIPE_READ_BLOCK_START (0x06)
#define FRL_PIPE_READ_BLOCK_COMPLETE (0x07)
#define FRL_PIPE_SEND_BLOCK_START (0x08)
#define FRL_PIPE_SEND_BLOCK_COMPLETE (0x09)
#define FRL_PIPE_DISABLED (0x0)
#define FRL_PIPE_WAIT_RESPONSE (0x1)

struct frl_pipe_state_t
{
	frl_pipe_header_t header;
	union {
		frl_pipe_block_t block;
		char buffer[SOCKET_PACKAGE_SIZE];
	};
	frl_pipe_data_t data;
	char* reader;
	apr_off_t offset;
	apr_size_t size;
	int state;
};

const apr_uint32_t SIZEOF_FRL_PIPE_HEADER_T = sizeof(frl_pipe_header_t);
const apr_uint32_t SIZEOF_FRL_PIPE_BLOCK_T = sizeof(frl_pipe_block_t);
const apr_uint32_t SIZEOF_FRL_PIPE_STATE_T = sizeof(frl_pipe_state_t);
const apr_uint32_t SIZEOF_FRL_PIPE_DATA_T = sizeof(frl_pipe_data_t);
const apr_uint32_t SIZEOF_FRL_PIPE_BLOCK_BUFFER = SOCKET_PACKAGE_SIZE-SIZEOF_FRL_PIPE_HEADER_T;

void* thread_socket_pipe_sender(apr_thread_t* thd, void* data);
void* thread_socket_pipe_receiver(apr_thread_t* thd, void* data);

class frl_socket_pipe
{
	friend void* thread_socket_pipe_sender(apr_thread_t* thd, void* data);
	friend void* thread_socket_pipe_receiver(apr_thread_t* thd, void* data);
	private:
		frl_slab_pool_t* datapool;
		frl_slab_pool_t* statepool;
		
		frl_queue_t* send_queue;
		
		apr_pool_t* mempool;
		apr_pool_t* sockpool;

		apr_thread_t* socket_pipe_sender;
		apr_thread_t* socket_pipe_receiver;
		apr_threadattr_t* thd_attr;
		
		apr_status_t recv_state;
		apr_status_t send_state;
		apr_uint32_t replicate;
		apr_sockaddr_t* sock_addr;
		bool destroyed;
		
		virtual apr_status_t send_after(char* buf, apr_size_t len)
		{
			return FRL_PROGRESS_CONTINUE;
		}
		virtual apr_status_t recv_before(char** buf, apr_size_t* len)
		{
			return FRL_PROGRESS_CONTINUE;
		}
		virtual apr_status_t recv_after(char* buf, apr_size_t len)
		{
			return FRL_PROGRESS_CONTINUE;
		}
	public:
		frl_socket_pipe(apr_size_t capacity, frl_lock_u lock, apr_pool_t* _mempool)
		{
			apr_pool_create(&mempool, _mempool);
			frl_slab_pool_create(&datapool, mempool, capacity, SIZEOF_FRL_PIPE_DATA_T, lock);
			frl_slab_pool_create(&statepool, mempool, capacity, SIZEOF_FRL_PIPE_STATE_T, lock);
			frl_queue_create(&send_queue, mempool, capacity, lock);
			apr_threadattr_create(&thd_attr, mempool);
			apr_pool_create(&sockpool, mempool);
			socket_pipe_receiver = 0;
			socket_pipe_sender = 0;
			destroyed = 0;
		}
		virtual ~frl_socket_pipe()
		{
			destroyed = 1;
			apr_pool_destroy(sockpool);
			frl_queue_destroy(send_queue);
			frl_slab_pool_destroy(datapool);
			frl_slab_pool_destroy(statepool);
			apr_pool_destroy(mempool);
		}
		void spawn(apr_uint32_t _replicate, apr_sockaddr_t* _sock_addr, int mode);
		void send(char* buf, apr_size_t len);
		apr_status_t state();
		void wait();
		void shutdown();
};

#endif
