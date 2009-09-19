/*************************
 * Fotas Runtime Library *
 * ***********************
 * accpet-reader model
 * author: Liu Liu
 */
#include "frl_util_multi_sock.h"

void* thread_accept( apr_thread_t* thd, void* data )
{
	int* idle_reader;
	bool quit_signal = 0;

	frl_multi_sock* ms = (frl_multi_sock*)data;
	apr_socket_t* listen_sock;
	apr_sockaddr_t* sock_addr;
	apr_sockaddr_info_get( &sock_addr, NULL, APR_INET, ms->port, 0, ms->mempool );
	apr_socket_create( &listen_sock, sock_addr->family, SOCK_STREAM, APR_PROTO_TCP, ms->mempool );
	apr_socket_opt_set( listen_sock, APR_SO_NONBLOCK, 0 );
	apr_socket_timeout_set( listen_sock, -1 );
	apr_socket_opt_set( listen_sock, APR_SO_REUSEADDR, 1 );

	apr_status_t rv;
	char rv_words[100];

	rv = apr_socket_bind( listen_sock, sock_addr );
	if ( rv != APR_SUCCESS )
	{
#ifdef FRL_DEBUGGING
		apr_strerror( rv, rv_words, 100 );
		printf( "[frl_multi_sock::thread_accept]: Socket Binding Error: Code: %d - %s\n", rv, rv_words );
#endif
		return NULL;
	}

	rv = apr_socket_listen( listen_sock,  SOMAXCONN );
	if ( rv != APR_SUCCESS )
	{
#ifdef FRL_DEBUGGING
		apr_strerror( rv, rv_words, 100 );
		printf( "[frl_multi_sock::thread_accept]: Socket Listen Error: Code: %d - %s\n", rv, rv_words );
#endif
		return NULL;
	}

	do {
		apr_socket_t* read_sock;
		rv = apr_socket_accept( &read_sock, listen_sock, ms->_sockpool);

		if ( rv != APR_SUCCESS )
		{
#ifdef FRL_DEBUGGING
			apr_strerror( rv, rv_words, 100 );
			printf( "[frl_multi_sock::thread_accept]: Socket Accept Error: Code: %d - %s\n", rv, rv_words );
#endif
			apr_sleep( DEF_ERROR_RETRY_TIME );
			continue;
		}

		apr_queue_pop( ms->_reader_queue, (void**)&idle_reader );
#ifdef FRL_DEBUGGING
		printf( "[frl_multi_sock::thread_accept]: No.%d Reader Selected.\n", *idle_reader );
#endif
		apr_thread_mutex_lock( ms->_reader_tasks[*idle_reader].mutex );
		ms->_reader_tasks[*idle_reader].socket = read_sock;
		ms->_reader_tasks[*idle_reader].status = 1;
		apr_thread_cond_signal( ms->_reader_tasks[*idle_reader].cond );
		apr_thread_mutex_unlock( ms->_reader_tasks[*idle_reader].mutex );
	} while (!quit_signal);
}

void* thread_read( apr_thread_t* thd, void* data )
{
	int* fmt_data = (int*)data;
	int id = fmt_data[0];
	frl_multi_sock* ms = (frl_multi_sock*)fmt_data[1];
	
	apr_status_t rv;
	char rv_words[100];

	bool quit_signal = 0;
	do {
		//wait message
		apr_thread_mutex_lock( ms->_reader_tasks[id].mutex );
		while ( !ms->_reader_tasks[id].status )
			apr_thread_cond_wait( ms->_reader_tasks[id].cond, ms->_reader_tasks[id].mutex );
		ms->_reader_tasks[id].status = 0;
		apr_thread_mutex_unlock( ms->_reader_tasks[id].mutex );
#ifdef FRL_DEBUGGING
		printf("[frl_multi_sock::thread_read]: %d Activated.\n", id);
#endif
		//process message
		if ( ms->proc( ms->_reader_tasks[id].socket, id ) == 0 )
		{
#ifdef FRL_DEBUGGING
			printf("[frl_multi_sock::thread_read]: %d Finished.\n", id);
#endif
		} else {
#ifdef FRL_DEBUGGING
			printf("[frl_multi_sock::thread_read]: %d Crashed!\n", id);
#endif
			quit_signal = 1;
		}
		//close sockeet
		rv = apr_socket_close( ms->_reader_tasks[id].socket );
		if ( rv != APR_SUCCESS )
		{
#ifdef FRL_DEBUGGING
			apr_strerror( rv, rv_words, 100 );
			printf( "[frl_multi_sock::thread_read]: Socket Close Error: Code: %d - %s\n", rv, rv_words );
#endif
			return NULL;
		}

		//push back to queue
		apr_queue_push( ms->_reader_queue, data );
	} while (!quit_signal);
}

apr_thread_t* frl_multi_sock::spawn( int n )
{
	apr_threadattr_create( &_thd_attr, mempool );
	_readers = (apr_thread_t**)apr_palloc( mempool, n*sizeof(apr_thread_t**) );
	_reader_tasks = (frl_task_t*)apr_pcalloc( mempool, n*sizeof(frl_task_t) );
	apr_queue_create( &_reader_queue, n, mempool );
	int* data = (int*)apr_palloc( mempool, n*(sizeof(int)+sizeof(frl_multi_sock**)) );
	int* iter_data = data;
	for ( int i = 0; i < n; i++, iter_data+=2 )
	{
		iter_data[0] = i;
		iter_data[1] = (int)this;
		_reader_tasks[i].status = 0;
		apr_thread_mutex_create( &_reader_tasks[i].mutex, APR_THREAD_MUTEX_UNNESTED, mempool );
		apr_thread_cond_create( &_reader_tasks[i].cond, mempool );
		apr_thread_create( &_readers[i], _thd_attr, thread_read, (void*)iter_data, mempool );
		apr_queue_push( _reader_queue, (void*)iter_data );
	}
	apr_thread_create( &_accept, _thd_attr, thread_accept, (void*)this, mempool );
	return _accept;
}

