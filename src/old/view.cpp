#include "cluscom.h"
#include <iostream>

int main()
{
	apr_initialize();
	apr_pool_t *mempool;
	apr_sockaddr_t *socket_addr;
	apr_socket_t *socket;
	apr_pool_create( &mempool, NULL );
	apr_sockaddr_info_get( &socket_addr, NULL, APR_INET, REPLY_PORT, 0, mempool );
	apr_socket_create( &socket, socket_addr->family, SOCK_STREAM, APR_PROTO_TCP, mempool );
	apr_socket_bind( socket, socket_addr );
	apr_socket_listen( socket, SOMAXCONN );
	apr_socket_t *accepted;
	apr_socket_accept( &accepted, socket, mempool );
	int *replies = (int*)malloc( frl_reply_size );
	apr_size_t len = frl_reply_size;
	do {
		apr_socket_recv( accepted, (char*)replies, &len );
		int *iter_replies = replies+2;
		for ( int i = 0; i < 100; i++, iter_replies+=2 )
		{
			std::cout<<*iter_replies<<" "<<*(iter_replies+1)<<std::endl;
		}
		std::cout<<"The End."<<std::endl;
	} while (1);
	apr_terminate();
	return 0;
}
