/*************************
 * Fotas Runtime Library *
 * ***********************
 * socket pipe with replications
 * author: Liu Liu
 */
#include "frl_util_socket_pipe.h"

void* thread_socket_pipe_receiver(apr_thread_t* thd, void* data)
{
	frl_socket_pipe* pipe = (frl_socket_pipe*)data;

	apr_status_t state;

	apr_socket_t* listen_sock;
	apr_socket_create(&listen_sock, pipe->sock_addr->family, SOCK_STREAM, APR_PROTO_TCP, pipe->sockpool);
	apr_socket_opt_set(listen_sock, APR_SO_NONBLOCK, 1);
	apr_socket_timeout_set(listen_sock, 0);
	apr_socket_opt_set(listen_sock, APR_SO_REUSEADDR, 1);
	pipe->recv_state = apr_socket_bind(listen_sock, pipe->sock_addr);
	F_ERROR_IF_RUN(APR_SUCCESS != pipe->recv_state, return NULL, "[frl_socket_pipe::thread_socket_pipe_receiver]: Socket Binding Error: %d\n", pipe->recv_state);
	pipe->recv_state = apr_socket_listen(listen_sock, SOMAXCONN);
	F_ERROR_IF_RUN(APR_SUCCESS != pipe->recv_state, return NULL, "[frl_socket_pipe::thread_socket_pipe_receiver]: Socket Listen Error: %d\n", pipe->recv_state);
	apr_uint32_t hash;
	apr_pollset_t* pollset;
	apr_pollset_create(&pollset, pipe->replicate+2, pipe->sockpool, 0);
	apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, NULL };
	pfd.desc.s = listen_sock;
	apr_pollset_add(pollset, &pfd);
	do {
		// the fun loop
		apr_int32_t total;
		const apr_pollfd_t* ret_pfd;
		pipe->recv_state = apr_pollset_poll(pollset, SOCKET_PIPE_POLL_TIMEOUT, &total, &ret_pfd);
		if (APR_SUCCESS == pipe->recv_state)
		{
			for (int i = 0; i < total; i++)
			{
				if (ret_pfd[i].desc.s == listen_sock)
				{
					apr_socket_t* accept_sock;
					state = apr_socket_accept(&accept_sock, listen_sock, pipe->sockpool);
					F_ERROR_IF_RUN(APR_SUCCESS != state, continue, "[frl_socket_pipe::thread_socket_pipe_receiver]: Socket Accept Error: %d\n", state);
					// accept connection, initiate recv
					frl_pipe_state_t* pipestate = (frl_pipe_state_t*)frl_slab_palloc(pipe->statepool);
					apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, pipestate };
					pipestate->state = FRL_PIPE_READ_HEADER_START;
					pipestate->reader = (char*)&pipestate->header;
					pipestate->offset = 0;
					pipestate->size = SIZEOF_FRL_PIPE_HEADER_T;
					pfd.desc.s = accept_sock;
					apr_socket_opt_set(accept_sock, APR_SO_NONBLOCK, 1);
					apr_socket_timeout_set(accept_sock, 0);
					apr_pollset_add(pollset, &pfd);
				} else {
					if (ret_pfd[i].rtnevents & APR_POLLIN)
					{
						frl_pipe_state_t* pipestate = (frl_pipe_state_t*)ret_pfd[i].client_data;
						apr_size_t len_a = pipestate->size-pipestate->offset;
						state = apr_socket_recv(ret_pfd[i].desc.s, pipestate->reader, &len_a);
						pipestate->offset += len_a;
						pipestate->reader += len_a;
						// read buffer to reader
						if ((pipestate->offset >= pipestate->size)||(APR_STATUS_IS_EAGAIN(state)))
						{
							pipestate->offset = pipestate->size;
							PIPE_STATE_TO_COMPLETE(pipestate->state);
							// read complete, move state to complete
						} else if ((APR_STATUS_IS_EOF(state))||(len_a == 0)) {
							apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, pipestate };
							pfd.desc.s = ret_pfd[i].desc.s;
							apr_pollset_remove(pollset, &pfd);
							frl_slab_pfree(pipestate);
							apr_socket_close(ret_pfd[i].desc.s);
							// remote error, close connection
							continue;
						}
						switch (pipestate->state)
						{
							case FRL_PIPE_READ_HEADER_COMPLETE:
							{
								// recv header (hash & size)
								pipestate->data.offset = 0;
								pipestate->data.size = pipestate->header.size;
								state = pipe->recv_before(&pipestate->data.buf, &pipestate->data.size);
								if (FRL_PROGRESS_IS_INTERRUPT(state))
								{
									apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, pipestate };
									pfd.desc.s = ret_pfd[i].desc.s;
									apr_pollset_remove(pollset, &pfd);
									frl_slab_pfree(pipestate);
									apr_socket_close(ret_pfd[i].desc.s);
									continue;
								}
								pipestate->state = FRL_PIPE_READ_BLOCK_START;
								// start to read block (<= 4092 bytes each)
								pipestate->reader = pipestate->buffer;
								pipestate->offset = 0;
								if (pipestate->data.size < SIZEOF_FRL_PIPE_BLOCK_BUFFER)
									pipestate->size = pipestate->data.size+SIZEOF_FRL_PIPE_HEADER_T;
								else
									pipestate->size = SOCKET_PACKAGE_SIZE;
								break;
							}
							case FRL_PIPE_READ_BLOCK_COMPLETE:
							{
								// a block complete, move to data
								memcpy(pipestate->data.buf+pipestate->data.offset, &pipestate->block.start, pipestate->block.header.size);
								hash = hashlittle(&pipestate->block.start, pipestate->size-SIZEOF_FRL_PIPE_HEADER_T);
								if (hash != pipestate->block.header.hash)
								{
									// check the hash fingerprint of the block
									apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, pipestate };
									pfd.desc.s = ret_pfd[i].desc.s;
									apr_pollset_remove(pollset, &pfd);
									frl_slab_pfree(pipestate);
									apr_socket_close(ret_pfd[i].desc.s);
									continue;
								}
								pipestate->data.offset += pipestate->block.header.size;
								if (pipestate->data.offset >= pipestate->data.size)
								{
									// finish read, report state to remote
									apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, pipestate };
									pfd.desc.s = ret_pfd[i].desc.s;
									apr_pollset_remove(pollset, &pfd);
									hash = hashlittle(pipestate->data.buf, pipestate->data.size);
									if (hash != pipestate->header.hash)
									{
										// check hash fingerprint of all data
										frl_slab_pfree(pipestate);
										apr_socket_close(ret_pfd[i].desc.s);
									} else {
										pfd.reqevents = APR_POLLOUT;
										state = pipe->recv_after(pipestate->data.buf, pipestate->data.size);
										if (FRL_PROGRESS_IS_INTERRUPT(state))
										{
											frl_slab_pfree(pipestate);
											apr_socket_close(ret_pfd[i].desc.s);
										} else {
											pipestate->state = FRL_PIPE_SEND_HEADER_START;
											pipestate->reader = (char*)&pipestate->header;
											pipestate->offset = 0;
											pipestate->size = SIZEOF_FRL_PIPE_HEADER_T;
											apr_pollset_add(pollset, &pfd);
										}
									}
									continue;
								}
								// to start read successor block
								pipestate->state = FRL_PIPE_READ_BLOCK_START;
								pipestate->reader = pipestate->buffer;
								pipestate->offset = 0;
								if (pipestate->data.size-pipestate->data.offset < SIZEOF_FRL_PIPE_BLOCK_BUFFER)
									pipestate->size = pipestate->data.size-pipestate->data.offset+SIZEOF_FRL_PIPE_HEADER_T;
								else
									pipestate->size = SOCKET_PACKAGE_SIZE;
								break;
							}
							default:
								break;
						}
					} else if (ret_pfd[i].rtnevents & APR_POLLOUT) {
						// send report information, basic header
						frl_pipe_state_t* pipestate = (frl_pipe_state_t*)ret_pfd[i].client_data;
						apr_size_t len_a = pipestate->size-pipestate->offset;
						state = apr_socket_send(ret_pfd[i].desc.s, pipestate->reader, &len_a);
						pipestate->offset += len_a;
						pipestate->reader += len_a;
						if ((pipestate->offset >= pipestate->size)||(APR_STATUS_IS_EAGAIN(state)))
						{
							pipestate->offset = pipestate->size;
							PIPE_STATE_TO_COMPLETE(pipestate->state);
						} else if ((APR_STATUS_IS_EOF(state))||(len_a == 0)) {
							apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLOUT, 0, { NULL }, pipestate };
							pfd.desc.s = ret_pfd[i].desc.s;
							apr_pollset_remove(pollset, &pfd);
							frl_slab_pfree(pipestate);
							apr_socket_close(ret_pfd[i].desc.s);
							continue;
						}
						switch (pipestate->state)
						{
							case FRL_PIPE_SEND_HEADER_COMPLETE:
							{
								// complete, return to listen state
								apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLOUT, 0, { NULL }, pipestate };
								pfd.desc.s = ret_pfd[i].desc.s;
								apr_pollset_remove(pollset, &pfd);
								pfd.reqevents = APR_POLLIN;
								pipestate->state = FRL_PIPE_DISABLED;
								pipestate->reader = 0;
								pipestate->offset = 0;
								pipestate->size = 0;
								apr_pollset_add(pollset, &pfd);
								break;
							}
							default:
								break;
						}
					} else {
						// other errors, close connection
						frl_pipe_state_t* pipestate = (frl_pipe_state_t*)ret_pfd[i].client_data;
						apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLIN | APR_POLLOUT, 0, { NULL }, pipestate };
						pfd.desc.s = ret_pfd[i].desc.s;
						apr_pollset_remove(pollset, &pfd);
						frl_slab_pfree(pipestate);
						apr_socket_close(ret_pfd[i].desc.s);
					}
				}
			}
		} else if (!APR_STATUS_IS_TIMEUP(pipe->recv_state)) {
			F_ERROR("[frl_socket_pipe::thread_socket_pipe_receiver]: Socket Poll Error: %d\n", pipe->recv_state);
			apr_sleep(SOCKET_PIPE_POLL_TIMEOUT);
			continue;
		}
	} while (!pipe->destroyed);
}

void* thread_socket_pipe_sender(apr_thread_t* thd, void* data)
{
	frl_socket_pipe* pipe = (frl_socket_pipe*)data;

	apr_status_t state;

	apr_pollset_t* pollset;

	apr_pollset_create(&pollset, pipe->replicate+1, pipe->sockpool, 0);

	apr_uint32_t success_conn = 0;
	apr_uint32_t write_available = 0;

	do {
		// push connections to epoll
		for (int i = success_conn; i < pipe->replicate+1; i++)
		{
			apr_socket_t* conn_sock;
			apr_socket_create(&conn_sock, pipe->sock_addr->family, SOCK_STREAM, APR_PROTO_TCP, pipe->sockpool);
			pipe->send_state = apr_socket_connect(conn_sock, pipe->sock_addr);
			if ((APR_SUCCESS == pipe->send_state)||(APR_EINPROGRESS == pipe->send_state))
			{
				apr_socket_opt_set(conn_sock, APR_SO_NONBLOCK, 1);
				apr_socket_timeout_set(conn_sock, 0);
				frl_pipe_state_t* pipestate = (frl_pipe_state_t*)frl_slab_palloc(pipe->statepool);
				apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLOUT, 0, { NULL }, pipestate };
				pipestate->state = FRL_PIPE_DISABLED;
				pipestate->reader = 0;
				pipestate->offset = 0;
				pipestate->size = 0;
				pfd.desc.s = conn_sock;
				apr_pollset_add(pollset, &pfd);
				success_conn++;
				write_available++;
			} else {
				F_WARNING("[frl_socket_pipe::thread_socket_pipe_sender]: Can not Establish Connection, Retry.\n");
			}
		}
		if (success_conn == 0)
			apr_sleep(SOCKET_PIPE_POLL_TIMEOUT);
		else {
			apr_int32_t total;
			const apr_pollfd_t* ret_pfd;
			pipe->send_state = apr_pollset_poll(pollset, SOCKET_PIPE_POLL_TIMEOUT, &total, &ret_pfd);
			F_ERROR_IF_RUN(pipe->send_state != APR_SUCCESS, continue, "[frl_socket_pipe::thread_socket_pipe_sender]: Socket Poll Error: %d\n", pipe->send_state);
			for (int i = 0; i < total; i++)
			{
				if (ret_pfd[i].rtnevents & APR_POLLOUT)
				{
					frl_pipe_state_t* pipestate = (frl_pipe_state_t*)ret_pfd[i].client_data;
					frl_pipe_data_t* sender;
					switch (pipestate->state)
					{
						case FRL_PIPE_DISABLED:
							if (write_available < success_conn)
								sender = (frl_pipe_data_t*)frl_queue_trypop(pipe->send_queue);
							else
								sender = (frl_pipe_data_t*)frl_queue_pop(pipe->send_queue);
							if (0 == sender)
								continue;
							if (write_available > 0)
								write_available--;
							pipestate->header.size = sender->size;
							pipestate->header.hash = hashlittle(sender->buf, sender->size);
							pipestate->data.buf = sender->buf;
							pipestate->data.offset = 0;
							pipestate->data.size = sender->size;
							frl_slab_pfree(sender);
							pipestate->state = FRL_PIPE_SEND_HEADER_START;
							pipestate->reader = (char*)&pipestate->header;
							pipestate->offset = 0;
							pipestate->size = SIZEOF_FRL_PIPE_HEADER_T;
							break;
						default:
							break;
					}
					apr_size_t len_a = pipestate->size-pipestate->offset;
					state = apr_socket_send(ret_pfd[i].desc.s, pipestate->reader, &len_a);
					pipestate->offset += len_a;
					pipestate->reader += len_a;
					if ((pipestate->offset >= pipestate->size)||(APR_STATUS_IS_EAGAIN(state)))
					{
						pipestate->offset = pipestate->size;
						PIPE_STATE_TO_COMPLETE(pipestate->state);
					} else if ((APR_STATUS_IS_EOF(state))||(len_a == 0)) {
						apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLOUT, 0, { NULL }, pipestate };
						pfd.desc.s = ret_pfd[i].desc.s;
						apr_pollset_remove(pollset, &pfd);
						pipe->send(pipestate->data.buf, pipestate->data.size);
						frl_slab_pfree(pipestate);
						apr_socket_close(ret_pfd[i].desc.s);
						if (success_conn > 0)
							success_conn--;
						continue;
					}
					switch (pipestate->state)
					{
						case FRL_PIPE_SEND_HEADER_COMPLETE:
						{
							pipestate->state = FRL_PIPE_SEND_BLOCK_START;
							pipestate->reader = pipestate->buffer;
							pipestate->offset = 0;
							if (pipestate->data.size < SIZEOF_FRL_PIPE_BLOCK_BUFFER)
								pipestate->block.header.size = pipestate->data.size;
							else
								pipestate->block.header.size = SIZEOF_FRL_PIPE_BLOCK_BUFFER;
							pipestate->size = pipestate->block.header.size+SIZEOF_FRL_PIPE_HEADER_T;
							memcpy(&pipestate->block.start, pipestate->data.buf, pipestate->block.header.size);
							pipestate->block.header.hash = hashlittle(&pipestate->block.start, pipestate->block.header.size);
							pipestate->data.offset += pipestate->block.header.size;
							break;
						}
						case FRL_PIPE_SEND_BLOCK_COMPLETE:
						{
							if (pipestate->data.offset < pipestate->data.size)
							{
								pipestate->state = FRL_PIPE_SEND_BLOCK_START;
								pipestate->reader = pipestate->buffer;
								pipestate->offset = 0;
								if (pipestate->data.size-pipestate->data.offset < SIZEOF_FRL_PIPE_BLOCK_BUFFER)
									pipestate->block.header.size = pipestate->data.size-pipestate->data.offset;
								else
									pipestate->block.header.size = SIZEOF_FRL_PIPE_BLOCK_BUFFER;
								pipestate->size = pipestate->block.header.size+SIZEOF_FRL_PIPE_HEADER_T;
								memcpy(&pipestate->block.start, pipestate->data.buf+pipestate->data.offset, pipestate->block.header.size);
								pipestate->block.header.hash = hashlittle(&pipestate->block.start, pipestate->block.header.size);
								pipestate->data.offset += pipestate->block.header.size;
							} else {
								apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLOUT, 0, { NULL }, pipestate };
								pfd.desc.s = ret_pfd[i].desc.s;
								apr_pollset_remove(pollset, &pfd);
								pfd.reqevents = APR_POLLIN;
								pipestate->data.hash = pipestate->header.hash;
								pipestate->state = FRL_PIPE_READ_HEADER_START;
								pipestate->reader = (char*)&pipestate->header;
								pipestate->offset = 0;
								pipestate->size = SIZEOF_FRL_PIPE_HEADER_T;
								apr_pollset_add(pollset, &pfd);
							}
							break;
						}
						default:
							break;
					}
				} else if (ret_pfd[i].rtnevents & APR_POLLIN) {
					frl_pipe_state_t* pipestate = (frl_pipe_state_t*)ret_pfd[i].client_data;
					apr_size_t len_a = pipestate->size-pipestate->offset;
					state = apr_socket_recv(ret_pfd[i].desc.s, pipestate->reader, &len_a);
					pipestate->offset += len_a;
					pipestate->reader += len_a;
					if ((pipestate->offset >= pipestate->size)||(APR_STATUS_IS_EAGAIN(state)))
					{
						pipestate->offset = pipestate->size;
						PIPE_STATE_TO_COMPLETE(pipestate->state);
					} else if ((APR_STATUS_IS_EOF(state))||(len_a == 0)) {
						apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, pipestate };
						pfd.desc.s = ret_pfd[i].desc.s;
						apr_pollset_remove(pollset, &pfd);
						pipe->send(pipestate->data.buf, pipestate->data.size);
						frl_slab_pfree(pipestate);
						apr_socket_close(ret_pfd[i].desc.s);
						if (success_conn > 0)
							success_conn--;
						continue;
					}
					switch (pipestate->state)
					{
						case FRL_PIPE_READ_HEADER_COMPLETE:
						{
							apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLIN, 0, { NULL }, pipestate };
							pfd.desc.s = ret_pfd[i].desc.s;
							apr_pollset_remove(pollset, &pfd);
							pfd.reqevents = APR_POLLOUT;
							if (pipestate->data.hash == pipestate->header.hash)
							{
								state = pipe->send_after(pipestate->data.buf, pipestate->data.size);
								// for user interrupt, do not reschedule sending
								if (FRL_PROGRESS_IS_INTERRUPT(state))
								{
									frl_slab_pfree(pipestate);
									apr_socket_close(ret_pfd[i].desc.s);
									if (success_conn > 0)
										success_conn--;
								} else {
									pipestate->state = FRL_PIPE_DISABLED;
									pipestate->reader = 0;
									pipestate->offset = 0;
									pipestate->size = 0;
									apr_pollset_add(pollset, &pfd);
								}
							} else {
								pipe->send(pipestate->data.buf, pipestate->data.size);
								frl_slab_pfree(pipestate);
								apr_socket_close(ret_pfd[i].desc.s);
								if (success_conn > 0)
									success_conn--;
							}
							break;
						}
						default:
							break;
					}
				} else {
					frl_pipe_state_t* pipestate = (frl_pipe_state_t*)ret_pfd[i].client_data;
					apr_pollfd_t pfd = { pipe->sockpool, APR_POLL_SOCKET, APR_POLLIN | APR_POLLOUT, 0, { NULL }, pipestate };
					pfd.desc.s = ret_pfd[i].desc.s;
					apr_pollset_remove(pollset, &pfd);
					if (FRL_PIPE_DISABLED != pipestate->state)
						pipe->send(pipestate->data.buf, pipestate->data.size);
					apr_socket_close(ret_pfd[i].desc.s);
					apr_socket_t* conn_sock;
					apr_socket_create(&conn_sock, pipe->sock_addr->family, SOCK_STREAM, APR_PROTO_TCP, pipe->sockpool);
					pipe->send_state = apr_socket_connect(conn_sock, pipe->sock_addr);
					if ((APR_SUCCESS != pipe->send_state)||(APR_EINPROGRESS != pipe->send_state))
					{
						apr_socket_opt_set(conn_sock, APR_SO_NONBLOCK, 1);
						apr_socket_timeout_set(conn_sock, 0);
						pfd.desc.s = conn_sock;
						pipestate->state = FRL_PIPE_DISABLED;
						pipestate->reader = 0;
						pipestate->offset = 0;
						pipestate->size = 0;
						apr_pollset_add(pollset, &pfd);
						write_available++;
					} else {
						F_WARNING("[frl_socket_pipe::thread_socket_pipe_sender]: Can not Establish Connection, Retry.\n");
						frl_slab_pfree(pipestate);
						apr_socket_close(conn_sock);
						if (success_conn > 0)
							success_conn--;
					}
				}
			}
		}
	} while (!pipe->destroyed);
}

void frl_socket_pipe::send(char* buf, apr_size_t len)
{
	frl_pipe_data_t* sender = (frl_pipe_data_t*)frl_slab_palloc(datapool);
	sender->buf = buf;
	sender->offset = 0;
	sender->size = len;
	frl_queue_push(send_queue, sender);
}

void frl_socket_pipe::spawn(apr_uint32_t _replicate, apr_sockaddr_t* _sock_addr, int mode)
{
	replicate = _replicate;
	sock_addr = _sock_addr;
	socket_pipe_sender = 0;
	socket_pipe_receiver = 0;
	if (mode & SOCKET_PIPE_SENDER)
		apr_thread_create(&socket_pipe_sender, thd_attr, thread_socket_pipe_sender, this, mempool);
	if (mode & SOCKET_PIPE_RECEIVER)
		apr_thread_create(&socket_pipe_receiver, thd_attr, thread_socket_pipe_receiver, this, mempool);
}

apr_status_t frl_socket_pipe::state()
{
	if (APR_SUCCESS != recv_state)
		return recv_state;
	if ((APR_SUCCESS != send_state)&&(APR_EINPROGRESS != send_state))
		return send_state;
	return APR_SUCCESS;
}

void frl_socket_pipe::wait()
{
	apr_status_t status;
	if (socket_pipe_sender)
		apr_thread_join(&status, socket_pipe_sender);
	if (socket_pipe_receiver)
		apr_thread_join(&status, socket_pipe_receiver);
}

void frl_socket_pipe::shutdown()
{
	apr_status_t retval;
	if (socket_pipe_sender)
		apr_thread_exit(socket_pipe_sender, retval);
	if (socket_pipe_receiver)
		apr_thread_exit(socket_pipe_receiver, retval);
	frl_slab_pool_clear(statepool);
}
