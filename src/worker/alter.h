#ifndef GUARD_alter_h
#define GUARD_alter_h

#include "../include/frl.h"
#include "../include/frl_util.h"
#include "../include/frl_logging.h"

struct frl_task_list_t
{
	void* task;
	frl_task_list_t* next;
};

struct frl_map_task_t
{
	frl_task_list_t* node;
	apr_uint64_t uid;
	apr_uint64_t tos;
	apr_uint32_t size;
};

struct frl_smart_pointer_t
{
	void* pointer;
	apr_uint32_t ref;
};

struct frl_ttl_watcher_t
{
	apr_time_t ttl;
	apr_uint64_t uid;
};

struct frl_request_header_t
{
	apr_uint64_t uid;
	apr_uint32_t size;
};

struct frl_request_t
{
	frl_request_header_t header;
	char start;
};

struct frl_response_header_t
{
	apr_uint64_t uid;
	apr_uint64_t tos;
	apr_uint32_t size;
};

struct frl_response_t
{
	frl_response_header_t header;
	char start;
};

struct frl_synthesize_header_t
{
	apr_uint64_t uid;
	apr_uint32_t total;
	frl_response_t** response;
	char** entry;
	apr_uint32_t* size;
};

const apr_uint32_t SIZEOF_FRL_TASK_LIST_T = sizeof(frl_task_list_t);
const apr_uint32_t SIZEOF_FRL_MAP_TASK_T = sizeof(frl_map_task_t);
const apr_uint32_t SIZEOF_FRL_SMART_POINTER_T = sizeof(frl_smart_pointer_t);
const apr_uint32_t SIZEOF_FRL_TTL_WATCHER_T = sizeof(frl_ttl_watcher_t);
const apr_uint32_t SIZEOF_FRL_REQUEST_HEADER_T = sizeof(frl_request_header_t);
const apr_uint32_t SIZEOF_FRL_REQUEST_T = sizeof(frl_request_t);
const apr_uint32_t SIZEOF_FRL_RESPONSE_HEADER_T = sizeof(frl_response_header_t);
const apr_uint32_t SIZEOF_FRL_RESPONSE_T = sizeof(frl_response_t);
const apr_uint32_t SIZEOF_FRL_SYNTHESIZE_HEADER_T = sizeof(frl_synthesize_header_t);
const apr_uint32_t SIZEOF_ALTER_REQUEST_CONF_T = sizeof(alter_request_conf_t);

struct alter_conf_t
{
	struct {
		apr_uint32_t uid;
		apr_uint32_t sid;
		apr_uint32_t follow;
		apr_uint32_t max;
		apr_uint32_t backup;
		apr_uint64_t tos;
		apr_uint64_t ttl;
	} network;
	struct {
		char server[40];
	} local;
	struct {
		apr_uint32_t capacity;
		frl_lock_u lock;
		frl_thread_model_u model;
	} performance;
};

struct alter_register_t
{
	apr_uint32_t ticket;
	char address[40];
	apr_uint32_t port;
};

struct alter_receipt_t
{
	apr_uint32_t ticket;
	apr_uint32_t sid;
	apr_uint32_t status;
};

#define FRL_INVALID_REQUEST (0)
#define FRL_VALID_REQUEST (1)

const apr_uint32_t SIZEOF_ALTER_CONF_T = sizeof(alter_conf_t);
const apr_uint32_t SIZEOF_ALTER_REGISTER_T = sizeof(alter_register_t);
const apr_uint32_t SIZEOF_ALTER_RECEIPT_T = sizeof(alter_receipt_t);

void* thread_alter_synthesizer(apr_thread_t* thd, void* data);
void* thread_ttl_watcher(apr_thread_t* thd, void* data);

class Alter
{
	class Allocator
	{
		private:
			apr_size_t min;
			apr_size_t total;
			frl_slab_pool_t** chain;
		public:
			Allocator(apr_size_t _min, apt_size_t _total, frl_lock_u lock, apr_pool_t* mempool)
			: min(_min),
			  total(_total)
			{
				chain = (frl_slab_pool_t**)apr_palloc(mempool, total*SIZEOF_POINTER);
				apr_uint32_t k = min;
				for (int i = 0; i < total; i++)
				{
					frl_slab_pool_create(&chain[i], mempool, 16, k, lock);
					k<<=1;
				}
			}
			~Allocator()
			{
				for (int i = 0; i < total; i++)
					frl_slab_pool_destroy(chain[i]);
			}
			void* alloc(apr_size_t size);
			void free(void* ptr);
	};

	class Configurator : public frl_server_event
	{
		private:
			frl_slab_pool_t* rgtpool;
			frl_slab_pool_t* rptpool;

			virtual apr_status_t recv_before(char** buf, apr_size_t* len);
			virtual apr_status_t recv_send(char** buf, apr_size_t* len, int* state, apr_time_t* timeout);
			virtual apr_status_t send_after(char* buf, apr_size_t len);
			Alter* alter;
		public:
			Configurator(Alter* _alter)
			: alter(_alter),
			  frl_server_event(_alter->conf.performance.capacity, _alter->conf.performance.lock, _alter->mempool)
			{
				frl_slab_pool_create(&rgtpool, _alter->mempool, _alter->conf.performance.capacity, SIZEOF_ALTER_REGISTER_T, _alter->conf.performance.lock);
				frl_slab_pool_create(&rptpool, _alter->mempool, _alter->conf.performance.capacity, SIZEOF_ALTER_RECEIPT_T, _alter->conf.performance.lock);
			}
			virtual ~Configurator()
			{
				frl_slab_pool_destroy(regpool);
			}
			apr_status_t signup();
	};

	class Reflector : public frl_socket_pipe
	{
		private:
			virtual apr_status_t recv_before(char** buf, apr_size_t* len);
			virtual apr_status_t recv_after(char* buf, apr_size_t len);
			virtual apr_status_t send_after(char* buf, apr_size_t len);
			Alter* alter;
		public:
			Reflector(Alter* _alter)
			: alter(_alter),
			  frl_socket_pipe(_alter->conf.performance.capacity, _alter->conf.performance.lock, _alter->mempool)
			{}
			virtual ~Reflector()
			{}
	};

	class Transporter : public frl_socket_pipe
	{
		private:
			virtual apr_status_t recv_before(char** buf, apr_size_t* len);
			virtual apr_status_t recv_after(char* buf, apr_size_t len);
			virtual apr_status_t send_after(char* buf, apr_size_t len);
			Alter* alter;
		public:
			Transporter(Alter* _alter)
			: alter(_alter),
			  frl_socket_pipe(_alter->conf.performance.capacity, _alter->conf.performance.lock, _alter->mempool)
			{}
			virtual ~Transporter()
			{}
	};

	class Handler : public frl_threads
	{
		private:
			Alter* alter;
			virtual apr_status_t execute(void* pointer);
		public:
			Handler(Alter* _alter)
			: alter(_alter),
			  frl_threads(_alter->conf.performance.model, _alter->conf.performance.lock, _alter->mempool)
			{}
			virtual ~Handler()
			{}
	};

	class Synthesizer : public frl_threads
	{
		friend void* thread_alter_synthesizer(apr_thread_t* thd, void* data);
		friend void* thread_ttl_watcher(apr_thread_t* thd, void* data);
		private:
			Alter* alter;
			frl_hash_t* hash;
			frl_queue_t* timers;
			frl_queue_t* packages;
			frl_slab_pool_t* synthepool;
			frl_slab_pool_t* taskpool;
			frl_slab_pool_t* watchpool;
			frl_slab_pool_t* mapool;
			virtual apr_status_t execute(void* pointer);
		public:
			Synthesizer(Alter* _alter)
			: alter(_alter),
			  frl_threads(_alter->conf.performance.model, _alter->conf.performance.lock, _alter->mempool)
			{
				frl_slab_pool_create(&synthepool, _alter->mempool, _alter->conf.performance.capacity, SIZEOF_FRL_SYNTHESIZE_HEADER_T, _alter->conf.performance.lock);
				frl_slab_pool_create(&taskpool, _alter->mempool, _alter->conf.performance.capacity, SIZEOF_FRL_TASK_LIST_T, _alter->conf.performance.lock);
				frl_slab_pool_create(&watchpool, _alter->mempool, _alter->conf.performance.capacity, SIZEOF_FRL_TTL_WATCHER_T, _alter->conf.performance.lock);
				frl_slab_pool_create(&mapool, _alter->mempool, _alter->conf.performance.capacity, SIZEOF_FRL_MAP_TASK_T, _alter->conf.performance.lock);
			}
			virtual ~Synthesizer()
			{
				frl_slab_pool_destroy(&synthepool);
				frl_slab_pool_destroy(&taskpool);
				frl_slab_pool_destroy(&watchpool);
				frl_slab_pool_destroy(&mapool);
			}
			apr_status_t synthesize(void* pointer);
			apr_status_t call(frl_map_task_t* map);
	};

	private:
		virtual int handle(char** response, apr_uint32_t* response_size, char* request, apr_uint32_t request_size) = 0;
		virtual int synthesize(char** response, apr_uint32_t* response_size, char** synthe, apr_uint32_t* synthe_size, apr_uint32_t total) = 0;

		Configurator* configurator;
		Reflector* reflector;
		Transporter** transporter;
		Handler* handler;
		Synthesizer* synthesizer;
		Allocator* ima;
		alter_conf_t conf;
		apr_pool_t* mempool;
	public:
		Alter(apr_pool_t* _mempool)
		{
			apr_pool_create(&mempool, _mempool);
			ima = new Allocator(mempool);
			memory = new Allocator(mempool);
		}
		void spawn();
		void wait();
		virtual ~Alter()
		{
			delete ima;
			delete memory;
			apr_pool_destroy(mempool);
		}
		Allocator* memory;
};
