#include "cluscom.h"
#include <iostream>

/* default socket timeout */
#define DEF_SOCK_TIMEOUT	(APR_USEC_PER_SEC * 30)

int main()
{
	apr_initialize();
	apr_pool_t* mempool;
	apr_sockaddr_t* socket_addr;
	apr_socket_t* socket;
	apr_pool_create( &mempool, NULL );
	apr_sockaddr_info_get( &socket_addr, "localhost", APR_INET, REQUEST_PORT, 0, mempool );
	apr_socket_create( &socket, socket_addr->family, SOCK_STREAM, APR_PROTO_TCP, mempool );
	apr_socket_opt_set( socket, APR_SO_NONBLOCK, 1 );
	apr_socket_timeout_set( socket, DEF_SOCK_TIMEOUT );
	apr_socket_connect( socket, socket_addr );
	int query[2];
	query[0] = 1;
	std::cin>>query[1];
	apr_size_t len = sizeof(query);
	apr_socket_send( socket, (char*)&query, &len );
	apr_socket_recv( socket, (char*)&query, &len );
	std::cout<<query[1]<<std::endl;
	apr_terminate();
	return 0;
}
