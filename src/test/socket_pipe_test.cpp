#include "../include/frl_util_socket_pipe.h"
#include "apr_mmap.h"
#include <iostream>

class FileTransfer : public frl_socket_pipe {
	private:
		virtual apr_status_t send_after(char* buf, apr_size_t len)
		{
			printf("Finish Send\n");
			return FRL_PROGRESS_CONTINUE;
		}
		virtual apr_status_t recv_before(char** buf, apr_size_t* len)
		{
			*buf = (char*)malloc(*len);
			printf("Start Recv\n");
			return FRL_PROGRESS_CONTINUE;
		}
		virtual apr_status_t recv_after(char* buf, apr_size_t len)
		{
			printf("Finish Recv\n");
			FILE* write = fopen(file, "w+");
			fwrite(buf, len, 1, write);
			fclose(write);
			free(buf);
			return FRL_PROGRESS_CONTINUE;
		}
		char* file;
	public:
		FileTransfer(char* _file, apr_size_t capacity, frl_lock_u lock, apr_pool_t* _mempool)
		: frl_socket_pipe(capacity, lock, _mempool),
		  file(_file)
		{}
};

int main(int argc, char** argv)
{
	apr_pool_t* mempool;
	apr_initialize();
	apr_pool_create(&mempool, NULL);
	apr_atomic_init(mempool);
	if (argc < 3)
		return -1;
	char* file = argv[2];
	apr_sockaddr_t* sockaddr;
	apr_sockaddr_info_get(&sockaddr, "127.0.0.1", APR_INET, 1020, 0, mempool);
	if (strcmp(argv[1], "send") == 0)
	{
		FileTransfer* ft = new FileTransfer(file, 100, FRL_LOCK_FREE, mempool);
		apr_file_t* afile;
		apr_status_t rv;
		rv = apr_file_open(&afile, file, APR_READ, APR_OS_DEFAULT, mempool);
		apr_finfo_t finfo;
		apr_file_info_get(&finfo, APR_FINFO_SIZE, afile);
		apr_mmap_t* mmap;
		rv = apr_mmap_create(&mmap, afile, 0, finfo.size, APR_MMAP_READ, mempool);
		char* map_addr = (char*)mmap->mm;
		ft->spawn(3, sockaddr, SOCKET_PIPE_SENDER);
		ft->send(map_addr, finfo.size);
		ft->wait();
	} else if (strcmp(argv[1], "recv") == 0) {
		FileTransfer* ft = new FileTransfer(file, 100, FRL_LOCK_FREE, mempool);
		ft->spawn(3, sockaddr, SOCKET_PIPE_RECEIVER);
		ft->wait();
	}
	apr_terminate();
	return 0;
}
