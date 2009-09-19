/*************************
 * Fotas Runtime Library *
 *************************
 * Author: Liu Liu
 */

#include "alter.h"

void* Alter::Allocator::alloc(apr_uint32_t size)
{
	apr_uint32_t k = min;
	for (int i = 0; i < total; i++)
	{
		if (size < k)
			return frl_slab_palloc(chain[i]);
		k<<=1;
	}
	return NULL;
}

void Alter::Allocator::free(void* ptr)
{
	frl_slab_pfree(ptr);
}

void Alter::Allocator::clear()
{
	for (int i = 0; i < total; i++)
		frl_slab_pool_clear(chain[i]);
}

apr_status_t Alter::Reflector::recv_before(char** buf, apr_uint32_t* len)
{
	*buf = (char*)+SIZEOF_FRL_SMART_POINTER_T;
	frl_smart_pointer_t* pointer = (frl_smart_pointer_t*)alter->ima->alloc(*len+SIZEOF_FRL_SMART_POINTER_T);
	pointer->ref = 0;
	pointer->pointer = pointer+1;
	*buf = (char*)pointer->pointer;
	return SOCKET_PIPE_CONTINUE;
}

apr_status_t Alter::Reflector::recv_after(char* buf, apr_uint32_t len)
{
	frl_smart_pointer_t* pointer = (frl_smart_pointer_t*)(buf-SIZEOF_FRL_SMART_POINTER_T);
	pointer->ref = alter->child+1;
	alter->handler->assign(buf);
	for (int i = 0; i < alter->conf.network.follow; i++)
		alter->transporter[i]->send(buf, len);
	return APR_SUCCESS;
}

apr_status_t Alter::Reflector::send_after(char* buf, apr_uint32_t len)
{
	frl_slab_pfree(buf);
	return APR_SUCCESS;
}

apr_status_t Alter::Transporter::recv_before(char** buf, apr_uint32_t* len)
{
	*buf = alter->ima->alloc(*len);
	return SOCKET_PIPE_CONTINUE;
}

apr_status_t Alter::Transporter::recv_after(char* buf, apr_uint32_t len)
{
	frl_response_t* response = (frl_response_t*)buf;
	response->header.tos = 1<<uid;
	alter->synthesizer->synthesize(response);
	return APR_SUCCESS;
}

apr_status_t Alter::Transporter::send_after(char* buf, apr_uint32_t len)
{
	frl_smart_pointer_t* pointer = (frl_smart_pointer_t*)(buf-SIZEOF_FRL_SMART_POINTER_T);
	apr_atomic_dec32(&pointer->ref);
	if (pointer->ref <= 0)
		frl_slab_pfree(pointer);
	return APR_SUCCESS;
}

apr_status_t Alter::Handler::execute(void* pointer)
{
	F_INFO("[Alter::Handler]: Entering Handler Thread.\n");
	frl_request_t* request = (frl_request_t*)pointer;
	char* buffer = 0;
	apr_uint32_t response_size = 0;
	int rv = alter->handle(&buffer, &response_size, &request->start, request->header.size);
	frl_response_t* response = (frl_response_t*)alter->ima->alloc(SIZEOF_FRL_RESPONSE_HEADER_T+response_size);
	response->header.uid = request->header.uid;
	response->header.size = response_size;
	response->header.tos = 1<<alter->conf.network.follow;
	memcpy(&response->start, buffer, response_size);
	alter->memory->free(buffer);

	F_INFO("[Alter::Handler]: Handled. Timestamp: %d.\n", request->uid);
	frl_smart_pointer_t* smart_pointer = (frl_smart_pointer_t*)pointer-1;
	apr_atomic_dec32(&smart_pointer->ref);
	if (smart_pointer->ref <= 0)
		frl_slab_pfree(smart_pointer);

	alter->synthesizer->synthesize(response);
	F_INFO("[Alter::Handler]: Pushed Result to Classifier.\n");
	return rv;
}

apr_status_t Alter::Synthesizer::execute(void* pointer)
{
	F_INFO("[Alter::Synthesizer]: Entering Synthesizer Thread.\n");
	frl_synthesize_header_t* synthesize = (frl_synthesize_header_t*)pointer;
	char* buffer = 0;
	apr_uint32_t response_size = 0;
	apr_status_t rv = alter->synthesize(&buffer, &response_size, synthesize->entry, synthesize->size, synthesize->total);
	F_INFO("[Alter::Synthesizer]: Synthesized. Timestamp: %d.\n", synthesize->uid);
	frl_response_t* response = (frl_response_t*)alter->ima->alloc(SIZEOF_FRL_RESPONSE_HEADER_T+response_size);
	response->header.uid = synthesize->uid;
	response->header.size = response_size;
	response->header.tos = 1<<alter->conf.network.sid;
	memcpy(&response->start, buffer, response_size);
	alter->memory->free(buffer);

	for (int i = 0; i < synthe->total; i++)
		alter->ima->free(synthesize->response[i]);
	alter->ima->free(synthesize->size);
	frl_slab_pfree(synthesize);
	Reflector->send(response, SIZEOF_FRL_RESPONSE_HEADER_T+response_size-1);
	F_INFO("[Alter::Synthesizer]: Pushed Result to Repeater.\n");
	return rv;
}

apr_status_t Alter::Synthesizer::synthesize(void* pointer)
{
	frl_queue_push(packages, pointer);
	return APR_SUCCESS;
}

apr_status_t Alter::Synthesizer::call(frl_map_task_t* map)
{
	frl_synthesize_header_t* synthesize = (frl_synthesize_header_t*)frl_slab_palloc(synthesizer->synthepool);
	synthesize->uid = map->uid;
	synthesize->total = map->size;
	synthesize->response = (frl_response_t**)alter->ima->alloc(map->size*SIZEOF_POINTER);
	synthesize->entry = (char**)alter->ima->alloc(map->size*SIZEOF_POINTER);
	synthesize->size = (apr_uint32_t*)alter->ima->alloc(map->size*SIZEOF_APR_UINT32_T);
	frl_task_list_t* node = 0;
	for (int i = 0; i < map->size; i++)
	{
		node = map->node;
		synthesize->response[i] = (frl_response_t*)node->task;
		synthesize->entry[i] = &synthesize->response[i]->start;
		synthesize->size[i] = synthesize->response[i]->header.size;
		map->node = map->node->next;
		frl_slab_pfree(node);
	}
	frl_hash_remove(hash, map->uid);
	frl_slab_pfree(map);
	F_INFO("[Alter::Synthesizer::call]:Timestamp: %d Gathered.\n", synthesize->uid);
	assign(synthesize);
	return APR_SUCCESS;
}

void* thread_alter_synthesizer(apr_thread_t* thd, void* data)
{
	Alter::Synthesizer* synthesizer = (Alter::Synthesizer*)data;
	do {
		frl_response_t* response = (frl_response_t*)frl_queue_pop(synthesizer->packages);
		frl_map_task_t* map = (frl_map_task_t*)frl_hash_get(synthesizer->hash, response->uid);
		if (response->tos == 0)
		{
			if (map != NULL)
				synthesizer->call(map);
			synthesizer->alter->ima->free(response);
			continue;
		} else if (map == NULL) {
			map = (frl_map_task_t*)frl_slab_palloc(synthesizer->mapool);
			map->uid = response->header.uid;
			map->size = 1;
			map->tos = response->header.tos;
			map->node = (frl_task_list_t*)frl_slab_palloc(synthesizer->taskpool);
			map->node->task = response;
			frl_hash_add(synthesizer->hash, response->uid, map);
			frl_ttl_watcher_t* ttl_watcher = (frl_ttl_watcher_t*)frl_slab_palloc(synthesizer->watchpool);
			ttl_watcher->uid = response->uid;
			ttl_watcher->ttl = apr_time_now()+synthesizer->alter->conf.network.ttl;
			frl_queue_push(synthesizer->timers, ttl_watcher);
		} else if (map->tos|response->tos != response->header.tos) {
			map->tos|=response->header.tos;
			map->size++;
			map->node->next = map->node;
			map->node = (frl_task_list_t*)frl_slab_palloc(synthesizer->taskpool);
			map->node->task = response;
		}
		if (map->tos == synthesizer->alter->conf.tos)
			synthesizer->call(map);
	} while (!synthesizer->destroyed);
}

void* thread_ttl_watcher(apr_thread_t* thd, void* data)
{
	Alter::Synthesizer* synthesizer = (Alter::Synthesizer*)data;
	bool quit_signal = false;
	apr_time_t time_now;
	do {
		frl_ttl_watcher_t* ttl_watcher = (frl_ttl_watcher_t*)frl_queue_pop(synthesizer->timers);
		time_now = apr_time_now();
		if (ttl_watcher->ttl > time_now)
			apr_sleep(ttl_watcher->ttl-time_now);
		if (frl_hash_get(synthesizer->pkg_hash, ttl_watcher->id) != NULL)
		{
			frl_response_t* response = (frl_response_header_t*)synthesizer->alter->ima->alloc(SIZEOF_FRL_RESPONSE_T);
			response->header.uid = ttl_watcher->uid;
			response->header.size = 0;
			response->header.tos = 0;
			synthesizer->synthesize(response);
		}
		frl_slab_pfree(ttl_watcher);
	} while (!synthesizer->destroyed);
}

apr_status_t Alter::Configurator::recv_before(char** buf, apr_size_t* len)
{
	*buf = frl_slab_palloc(rgtpool);
	*len = SIZEOF_ALTER_REGISTER_T;
	return FRL_PROGRESS_CONTINUE;
}

apr_status_t Alter::Configurator::recv_send(char** buf, apr_size_t* len, int* state, apr_time_t* timeout)
{
	if (*len < SIZEOF_ALTER_REGISTER_T)
		return SERVER_EVENT_CONTINUE;
	alter_register_t* rgt = (alter_register_t*)*buf;
	apr_uint32_t sid = alter->conf.network.max;
	if (alter->conf.network.follow < alter->conf.network.max)
	{
		apr_sockaddr_t* sockaddr;
		apr_sockaddr_info_get(&sockaddr, rgt->address, APR_INET, rgt->port, 0, mempool);
		sid = alter->conf.network.follow;
		alter->transporter[sid].spawn(alter->conf.network.backup, sockaddr, SOCKET_PIPE_SENDER & SOCKET_PIPE_RECEIVER);
		alter->conf.network.follow++;
	} else {
		for (int i = 0; i < alter->conf.network.max)
			if (APR_SUCCESS != alter->transporter[i]->state())
			{
				sid = i;
				alter->transporter[sid]->shutdown();
				apr_sockaddr_t* sockaddr;
				apr_sockaddr_info_get(&sockaddr, rgt->address, APR_INET, rgt->port, 0, mempool);
				alter->transporter[sid].spawn(alter->conf.network.backup, sockaddr, SOCKET_PIPE_SENDER & SOCKET_PIPE_RECEIVER);
				break;
			}
	}
	alter_receipt_t* rpt = (alter_receipt_t*)frl_slab_palloc(rptpool);
	rpt->ticket = rgt->ticket;
	rpt->sid = sid;
	if (sid < alter->conf.network.max)
		rpt->status = FRL_VALID_REQUEST;
	else
		rpt->status = FRL_INVALID_REQUEST;
	*buf = (char*)rpt;
	*len = SIZEOF_ALTER_RECEIPT_T;
	return FRL_PROGRESS_COMLETE;
}

apr_status_t Alter::Configurator::send_after(char* buf, apr_size_t len)
{
	frl_slab_pfree(buf);
	return FRL_PROGRESS_INTERRUPT;
}

apr_status_t Alter::Configurator::signup(apr_sockaddr_t sockaddr)
{
	apr_sockaddr_t* sockaddr;
	apr_sockaddr_info_get(&sockaddr, alter->conf.local.server, APR_INET, 0, mempool);
	apr_socket_t* server;
	apr_socket_create(&server, sockaddr->family, SOCK_STREAM, APR_PROTO_TCP, mempool);
}

void Alter::spawn()
{
}

void Alter::wait()
{
}
