#include "SweetSocket.h"
#include "SweetSocket_Threadeds.h"

static bool SweetSocket_serverInvalidAction(struct SweetSocket_global_context *context)
{
	return context == NULL || context->type == PEER_CLIENT || context->status == STATUS_NOT_INIT || context->status == STATUS_CLOSED || context->status == STATUS_IN_CLOSE || context->connections.size <= 0 || context->connections.base == NULL;
	;
}

EXPORT bool SweetSocket_serverStartAccepting(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID, enum SweetSocket_sweet_callback_status (*functionSend)(void),  enum SweetSocket_sweet_callback_status (*functionRecv)(char *, uint64_t, struct SweetSocket_global_context *, struct SweetSocket_peer_clients *, void *), void *parmsRecv, void *parmsSend, enum SweetSocket_peer_pool_behaviour pool)
{
	if (SweetSocket_serverInvalidAction(context))
		return false;
	for (struct SweetSocket_peer_connects *current = context->connections.base; current != NULL; current = current->next)
	{
		if (!(serverID == APPLY_ALL ? true : current->id == serverID ? true
																	 : false) ||
			current->socket.socket == 0)
			continue;
		struct SweetSocket_accept_data_context_thread *acceptContext = (struct SweetSocket_accept_data_context_thread *)calloc(1, sizeof(struct SweetSocket_accept_data_context_thread));
		acceptContext->context = context;
		acceptContext->connection = current;
		acceptContext->connection->enableRecivePool = (pool == ONLY_RECIVE || pool == BOTH) ? true : false;
		acceptContext->connection->enableSendPool = (pool == ONLY_SEND || pool == BOTH) ? true : false;
		acceptContext->context->status = STATUS_INIT;
		acceptContext->functionRecv = (void*)functionRecv;
		acceptContext->functionSend = (void *)functionSend;
		acceptContext->intoExternaParmRecv = parmsRecv;
		acceptContext->intoExternaParmSend = parmsSend;
		acceptContext->connection->acceptThread = SweetThread_createThread(SweetSocket_acceptConnectionThread, (void *)acceptContext, true);
	}
	return true;
}

EXPORT bool SweetSocket_serverStartListening(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID)
{
	if (context == NULL || !(context->type == PEER_SERVER) || context->connections.size == 0)
		return false;
	for (struct SweetSocket_peer_connects *current = context->connections.base; current != NULL; current = current->next)
	{
		if (!((current->socket.type == AF_INET) ^ (current->socket.type == AF_INET6)) ||
			(current->socket.socket != 0) ||
			!(serverID == APPLY_ALL ? true : current->id == serverID ? true
																	 : false))
			continue;
		struct addrinfo hints = {0};
		hints.ai_family = current->socket.type;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;
		struct addrinfo *result = NULL;
		if (!SweetSocket_peerOpenSocket(current->socket.addr, current->socket.port, &hints, &result, &current->socket.socket))
		{
			// Failed
			continue;
		}
		if (bind(current->socket.socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
		{
			// Failed
			freeaddrinfo(result);
			return false;
		}
		freeaddrinfo(result);
		if (listen(current->socket.socket, SOMAXCONN) == SOCKET_ERROR)
		{
			// Failed
			continue;
		}
	}
	return true;
}
