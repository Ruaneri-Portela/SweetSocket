#include "SweetSocket.h"
#include "SweetSocket_Threadeds.h"

static bool SweetSocket_serverInvalidAction(struct SweetSocket_global_context *context)
{
	return context == NULL || context->type == PEER_SERVER || context->status == STATUS_NOT_INIT || context->status == STATUS_CLOSED || context->status == STATUS_IN_CLOSE || context->connections.size <= 0 || context->connections.base == NULL;
	;
}

EXPORT bool SweetSocket_clientStartConnection(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID)
{
	if (SweetSocket_serverInvalidAction(context))
		return false;
	for (struct SweetSocket_peer_connects *current = context->connections.base; current != NULL; current = current->next)
	{
		if (!(serverID == APPLY_ALL ? true : current->id == serverID ? true
																	 : false))
			continue;
		struct addrinfo hints = {0};
		hints.ai_family = current->socket.type;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		struct addrinfo *result = NULL;
		if (!SweetSocket_peerOpenSocket(current->socket.addr, current->socket.port, &hints, &result, &current->socket.socket))
		{
			// Failed
			continue;
		}
		if (connect(current->socket.socket, result->ai_addr, result->ai_addrlen) == SOCKET_ERROR)
		{
			// Failed
			closesocket(current->socket.socket);
			freeaddrinfo(result);
			continue;
		}
		freeaddrinfo(result);
		context->connectionsAlive++;
		context->status = STATUS_INIT;
	}
	return true;
}

EXPORT bool SweetSocket_clientEnablePools(struct SweetSocket_global_context *context, enum SweetSocket_apply_on clientID, bool enableRecivePool, bool enableSendPool)
{
	if (SweetSocket_serverInvalidAction(context))
		return false;
	for (struct SweetSocket_peer_clients *current = context->clients; current != NULL; current = current->next)
	{
		if (!(clientID == APPLY_ALL ? true : current->id == clientID ? true
																	 : false))
			continue;
		if (enableRecivePool && current->reciveThread.address == NULL)
		{
			struct SweetSocket_data_context_thread *reciveContext = (struct SweetSocket_data_context_thread *)calloc(1, sizeof(struct SweetSocket_data_context_thread));
			reciveContext->context = context;
			reciveContext->connection = current;
			reciveContext->connection->reciveThread = SweetThread_createThread(SweetSocket_reciveThread, reciveContext, true);
		}
		if (enableSendPool && current->sendThread.address == NULL)
		{
			struct SweetSocket_data_context_thread *sendContext = (struct SweetSocket_data_context_thread *)calloc(1, sizeof(struct SweetSocket_data_context_thread));
			sendContext->context = context;
			sendContext->connection = current;
			sendContext->connection->sendThread = SweetThread_createThread(SweetSocket_sendThread, sendContext, true);
		}
	}
	return true;
}