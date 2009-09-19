/*************************
 * Fotas Runtime Library *
 * ***********************
 * socket server with event-driven function
 * author: Liu Liu
 */
#include "frl_util_server_event.h"

void* thread_server_event_listener(apr_thread_t* thd, void* data)
{
	frl_server_event* event = (frl_server_event*)data;

	apr_status_t rv;

	apr_socket_t* listen_sock;
	apr_socket_create(&listen_sock, event->sock_addr->family, SOCK_STREAM, APR_PROTO_TCP, event->mempool);
	apr_socket_opt_set(listen_sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(listen_sock, -1);
	apr_socket_opt_set(listen_sock, APR_SO_REUSEADDR, 1);
	rv = apr_socket_bind(listen_sock, event->sock_addr);
	F_ERROR_IF_RUN(rv != APR_SUCCESS, return NULL, "[frl_server_event::thread_server_event_listen]: Socket Binding Error: %d\n", rv);
	rv = apr_socket_listen(listen_sock,  SOMAXCONN);
	F_ERROR_IF_RUN(rv != APR_SUCCESS, return NULL, "[frl_server_event::thread_server_event_listen]: Socket Listen Error: %d\n", rv);

	do {
		apr_socket_t* accept_sock;
		rv = apr_socket_accept(&accept_sock, listen_sock, event->mempool);
		F_ERROR_IF_RUN(rv != APR_SUCCESS, continue, "[frl_server_event::thread_server_event_listen]: Socket Accept Error: %d\n", rv);
		apr_pollset_t* select = event->pollset[0];
		apr_uint32_t* min = event->balance;
		for (int i = 1; i < event->open; i++)
			if (event->balance[i] < *min)
			{
				min = event->balance+i;
				select = event->pollset[i];
			}
		if ((*min > 0)&&(event->open < event->max))
		{
			event->labor[event->open].no = event->open;
			event->labor[event->open].master = event;
			event->balance[event->open] = 0;
			apr_pollset_create(&event->pollset[event->open], event->capacity, event->sockpool, APR_POLLSET_THREADSAFE);
			apr_thread_create(event->server_event_handler_thds+event->open, event->thd_attr, thread_server_event_handler, event->labor+event->open, event->mempool);
			min = event->balance+event->open;
			event->open++;
		}
		(*min)++;
		frl_eventinfo_t* eventinfo = (frl_eventinfo_t*)frl_slab_palloc(event->eventpool);
		apr_pollfd_t pfd = { event->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, eventinfo };
		eventinfo->state = FRL_EVENT_DISABLED;
		pfd.desc.s = accept_sock;
		apr_socket_opt_set(accept_sock, APR_SO_NONBLOCK, 1);
		apr_socket_timeout_set(accept_sock, 0);
		apr_pollset_add(select, &pfd);
	} while (!event->destroyed);
}

void* thread_server_event_handler(apr_thread_t* thd, void* data)
{
	frl_server_labor_t* labor = (frl_server_labor_t*)data;
	frl_server_event* event = labor->master;

	apr_status_t rv;

	apr_pollset_t* pollset = event->pollset[labor->no];
	apr_uint32_t& balance = event->balance[labor->no];
	do {
		apr_int32_t total;
		const apr_pollfd_t* ret_pfd;
		rv = apr_pollset_poll(pollset, SERVER_EVENT_POLL_TIMEOUT, &total, &ret_pfd);
		if (rv == APR_SUCCESS)
		{
			for (int i = 0; i < total; i++)
			{
				if (ret_pfd[i].rtnevents & APR_POLLIN)
				{ 
					frl_eventinfo_t* eventinfo = (frl_eventinfo_t*)ret_pfd[i].client_data;
					switch (eventinfo->state)
					{
						case FRL_EVENT_DISABLED:
						{
							rv = event->recv_before(&eventinfo->notify.buffer, &eventinfo->notify.size);
							if (FRL_PROGRESS_IS_INTERRUPT(rv))
							{
								apr_pollfd_t pfd = { event->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, eventinfo };
								pfd.desc.s = ret_pfd[i].desc.s;
								apr_pollset_remove(pollset, &pfd);
								frl_slab_pfree(eventinfo);
								apr_socket_close(ret_pfd[i].desc.s);
								balance--;
								continue;
							}
							eventinfo->state = FRL_EVENT_READ_START;
							eventinfo->reader = eventinfo->notify.buffer;
							eventinfo->offset = 0;
							eventinfo->size = eventinfo->notify.size;
							break;
						}
						default:
							break;
					}
					apr_size_t len_a = eventinfo->size-eventinfo->offset;
					rv = apr_socket_recv(ret_pfd[i].desc.s, eventinfo->reader, &len_a);
					eventinfo->offset += len_a;
					eventinfo->reader += len_a;
					// read buffer to reader
					if ((APR_STATUS_IS_EOF(rv))||(len_a == 0))
					{
						apr_pollfd_t pfd = { event->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, eventinfo };
						pfd.desc.s = ret_pfd[i].desc.s;
						apr_pollset_remove(pollset, &pfd);
						frl_slab_pfree(eventinfo);
						apr_socket_close(ret_pfd[i].desc.s);
						// remote error, close connection
						balance--;
						continue;
					}
					eventinfo->notify.size = eventinfo->offset;
					eventinfo->notify.timeout = 0;
					rv = event->recv_send(&eventinfo->notify.buffer, &eventinfo->notify.size, &eventinfo->notify.state, &eventinfo->notify.timeout);
					if (!FRL_PROGRESS_IS_CONTINUE(rv))
					{
						apr_pollfd_t pfd = { event->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, eventinfo };
						pfd.desc.s = ret_pfd[i].desc.s;
						switch (rv)
						{
							case FRL_PROGRESS_COMLETE:
								apr_pollset_remove(pollset, &pfd);
								pfd.reqevents = APR_POLLOUT;
								eventinfo->state = FRL_EVENT_SEND_START;
								eventinfo->reader = eventinfo->notify.buffer;
								eventinfo->offset = 0;
								eventinfo->size = eventinfo->notify.size;
								apr_pollset_add(pollset, &pfd);
								break;
							case FRL_PROGRESS_INTERRUPT:
								apr_pollset_remove(pollset, &pfd);
								frl_slab_pfree(eventinfo);
								apr_socket_close(ret_pfd[i].desc.s);
								break;
							case FRL_PROGRESS_WAIT_SIGNAL:
								apr_pollset_remove(pollset, &pfd);
								pfd.reqevents = APR_POLLOUT;
								eventinfo->state = FRL_EVENT_WAIT_RESPONSE;
								apr_pollset_add(pollset, &pfd);
								break;
							case FRL_PROGRESS_RESTART:
								eventinfo->state = FRL_EVENT_READ_START;
								eventinfo->reader = eventinfo->notify.buffer;
								eventinfo->offset = 0;
								eventinfo->size = eventinfo->notify.size;
								break;
							default:
								break;
						}
					}
				} else if (ret_pfd[i].rtnevents & APR_POLLOUT) {
					frl_eventinfo_t* eventinfo = (frl_eventinfo_t*)ret_pfd[i].client_data;
					if ((eventinfo->notify.timeout > 0)&&(apr_time_now() > eventinfo->notify.timeout))
					{
						apr_pollfd_t pfd = { event->sockpool, APR_POLL_SOCKET, APR_POLLOUT, 0, { NULL }, eventinfo };
						pfd.desc.s = ret_pfd[i].desc.s;
						apr_pollset_remove(pollset, &pfd);
						frl_slab_pfree(eventinfo);
						apr_socket_close(ret_pfd[i].desc.s);
						balance--;
						continue;
					}
					if ((FRL_EVENT_WAIT_RESPONSE == eventinfo->state)&&(FRL_PROGRESS_IS_COMLETE(eventinfo->notify.state)))
					{
						eventinfo->state = FRL_EVENT_SEND_START;
						eventinfo->reader = eventinfo->notify.buffer;
						eventinfo->offset = 0;
						eventinfo->size = eventinfo->notify.size;
					}
					apr_size_t len_a = eventinfo->size-eventinfo->offset;
					rv = apr_socket_send(ret_pfd[i].desc.s, eventinfo->reader, &len_a);
					eventinfo->offset += len_a;
					eventinfo->reader += len_a;
					if ((eventinfo->offset >= eventinfo->size)||(APR_STATUS_IS_EAGAIN(rv)))
					{
						eventinfo->offset = eventinfo->size;
						EVENT_STATE_TO_COMPLETE(eventinfo->state);
					} else if ((APR_STATUS_IS_EOF(rv))||(len_a == 0)) {
						apr_pollfd_t pfd = { event->sockpool, APR_POLL_SOCKET, APR_POLLOUT, 0, { NULL }, eventinfo };
						pfd.desc.s = ret_pfd[i].desc.s;
						apr_pollset_remove(pollset, &pfd);
						frl_slab_pfree(eventinfo);
						apr_socket_close(ret_pfd[i].desc.s);
						balance--;
						continue;
					}
					switch (eventinfo->state)
					{
						case FRL_EVENT_SEND_COMPLETE:
						{
							rv = event->send_after(eventinfo->notify.buffer, eventinfo->notify.size);
							apr_pollfd_t pfd = { event->sockpool, APR_POLL_SOCKET, APR_POLLOUT, 0, { NULL }, eventinfo };
							pfd.desc.s = ret_pfd[i].desc.s;
							switch (rv)
							{
								case FRL_PROGRESS_COMLETE:
									apr_pollset_remove(pollset, &pfd);
									pfd.reqevents = APR_POLLIN;
									eventinfo->state = FRL_EVENT_DISABLED;
									eventinfo->reader = 0;
									eventinfo->offset = 0;
									eventinfo->size = 0;
									apr_pollset_add(pollset, &pfd);
									break;
								case FRL_PROGRESS_INTERRUPT:
									apr_pollset_remove(pollset, &pfd);
									frl_slab_pfree(eventinfo);
									apr_socket_close(ret_pfd[i].desc.s);
									break;
								case FRL_PROGRESS_CONTINUE:
									eventinfo->state = FRL_EVENT_SEND_START;
									eventinfo->reader = eventinfo->notify.buffer;
									eventinfo->offset = 0;
									eventinfo->size = eventinfo->notify.size;
									break;
								default:
									break;
							}
							break;
						}
						default:
							break;
					}
				} else {
					frl_eventinfo_t* eventinfo = (frl_eventinfo_t*)ret_pfd[i].client_data;
					apr_pollfd_t pfd = { event->sockpool, APR_POLL_SOCKET, APR_POLLIN | APR_POLLOUT, 0, { NULL }, eventinfo };
					pfd.desc.s = ret_pfd[i].desc.s;
					apr_pollset_remove(pollset, &pfd);
					frl_slab_pfree(eventinfo);
					apr_socket_close(ret_pfd[i].desc.s);
					balance--;
				}
			}
		} else if (!APR_STATUS_IS_TIMEUP(rv)) {
			F_ERROR("[frl_socket_event::thread_socket_event_handler]: Socket Poll Error: Code: %d\n", rv);
			continue;
		}
	} while (!event->destroyed);
}

void frl_server_event::spawn(apr_uint32_t _min, apr_uint32_t _max, apr_sockaddr_t* _sock_addr)
{
	min = _min;
	max = _max;
	open = min;
	sock_addr = _sock_addr;
	server_event_handler_thds = (apr_thread_t**)apr_palloc(mempool, max*SIZEOF_POINTER);
	labor = (frl_server_labor_t*)apr_palloc(mempool, max*SIZEOF_FRL_SERVER_LABOR_T);
	balance = (apr_uint32_t*)apr_palloc(mempool, max*SIZEOF_APR_UINT32_T);
	pollset = (apr_pollset_t**)apr_palloc(mempool, max*SIZEOF_POINTER);
	apr_thread_create(&server_event_listen_thd, thd_attr, thread_server_event_listener, this, mempool);
	for (int i = 0; i < min; i++)
	{
		labor[i].no = i;
		labor[i].master = this;
		balance[i] = 0;
		apr_pollset_create(pollset+i, capacity, sockpool, APR_POLLSET_THREADSAFE);
		apr_thread_create(server_event_handler_thds+i, thd_attr, thread_server_event_handler, labor+i, mempool);
	}
}

void frl_server_event::wait()
{
	apr_status_t status;
	apr_thread_join(&status, server_event_listen_thd);
}
