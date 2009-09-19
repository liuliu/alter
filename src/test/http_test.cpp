#include <iostream>
#include "../include/frl_util_server_event.h"

const char http_ok[] = "HTTP/1.1 200 OK\r\nServer: http-test/0.1.0\r\nContent-Type: text/html\r\nContent-Length: 67\r\n\r\n<head><head><title>index.html</title></head><body>index.html</body>\0";

class HttpServer : public frl_server_event
{
	private:
		apr_byte_t buf[4096];
		virtual apr_status_t recv_before(char** buf, apr_size_t* len)
		{
			printf("Recv Before\n");
			for (int i = 0; i < 4096; i++)
				this->buf[i] = 0;
			*buf = (char*)this->buf;
			*len = 4096;
			return FRL_PROGRESS_CONTINUE;
		}
		virtual apr_status_t recv_send(char** buf, apr_size_t* len, int* state, apr_time_t* timeout)
		{
			printf("Recv Send\n");
			if (*len >= 4096)
				return FRL_PROGRESS_INTERRUPT;
			printf("%s\n", *buf);
			for (int i = 0; i < 4096; i++)
				(*buf)[i] = 0;
			memcpy(*buf, http_ok, sizeof(http_ok));
			*len = sizeof(http_ok);
			return FRL_PROGRESS_COMLETE;
		}
		virtual apr_status_t send_after(char* buf, apr_size_t len)
		{
			printf("Send After\n");
			return FRL_PROGRESS_COMLETE;
		}
	public:
		HttpServer(apr_uint32_t _capacity, frl_lock_u lock, apr_pool_t* _mempool)
		: frl_server_event(_capacity, lock, _mempool)
		{}
};

int main()
{
	apr_pool_t* mempool;
	apr_initialize();
	apr_pool_create(&mempool, NULL);
	apr_atomic_init(mempool);
	apr_sockaddr_t* sockaddr;
	apr_sockaddr_info_get(&sockaddr, "127.0.0.1", APR_INET, 8080, 0, mempool);
	HttpServer* hs = new HttpServer(100, FRL_LOCK_FREE, mempool);
	hs->spawn(5, 10, sockaddr);
	hs->wait();
	apr_terminate();
	return 0;
}
