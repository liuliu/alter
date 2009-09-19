#include "cluscom-interface.h"

int
cluscom_query( char* host,
	       int port,
	       char* input,
	       int len_in,
	       char* output,
	       int len_out,
	       apr_pool_t* mempool )
{
	apr_socket_t* sock;
	apr_sockaddr_t* sock_addr;
	apr_sockaddr_info_get(&sock_addr, host, APR_INET, port, 0, mempool);
	apr_socket_create(&sock, sock_addr->family, SOCK_STREAM, APR_PROTO_TCP, mempool);
	apr_socket_opt_set(sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(sock, DEF_SOCKET_TIMEOUT);
	apr_socket_opt_set(sock, APR_SO_REUSEADDR, 1);

	apr_status_t rv;
	char rv_words[100];
	rv = apr_socket_connect(sock, sock_addr);
	if (rv != APR_SUCCESS)
	{
#ifdef FRL_DEBUGGING
		apr_strerror(rv, rv_words, 100);
		printf("[cluscom_query]: Socket Connect Error: Code: %d - %s\n", rv, rv_words);
#endif
		apr_socket_close(sock);
		return -1;
	}
	apr_uint32_t len_in_a = len_in;
	rv = apr_socket_send(sock, input, &len_in_a);
	if ((rv == APR_SUCCESS)&&(len_in_a == len_in))
	{
		apr_uint32_t len_out_a = len_out;
		apr_socket_timeout_set(sock, DEF_PROCESS_TIMEOUT);
		rv = apr_socket_recv(sock, output, &len_out_a);
		apr_socket_close(sock);
		if ((rv != APR_SUCCESS)||(len_out_a != len_out))
		{
#ifdef FRL_DEBUGGING
			if (rv != APR_SUCCESS)
			{
				apr_strerror(rv, rv_words, 100);
				printf("[cluscom_query]: Socket Receive Error: Code: %d - %s\n", rv, rv_words);
			} else {
				printf("[cluscom_query]: Socket Receive Warning: Not Correct Size.\n");
			}
#endif
			return -1;
		}
	} else {
#ifdef FRL_DEBUGGING
		if (rv != APR_SUCCESS)
		{
			apr_strerror(rv, rv_words, 100);
			printf("[cluscom_query]: Socket Send Error: Code: %d - %s\n", rv, rv_words);
		} else {
			printf("[cluscom_query]: Socket Send Warning: Not Correct Size.\n");
		}
#endif
		apr_socket_close(sock);
		return -1;
	}
	return 0;
}
