#include "SweetSocket.h"
#include "SweetSocket_Threadeds.h"
#include <stdio.h>

#ifdef WINSWEETSOCKET
WSADATA wsaData = {0};
#endif

static uint64_t SweetSocket_findMinorId(struct SweetSocket_peers *conn)
{
	uint64_t minor = 1;
	if (conn == NULL || conn->size == 0 || conn->base == NULL)
		return minor;
	struct SweetSocket_peer_connects *current = conn->base;
	while (current != NULL)
	{
		if (current->id == minor)
		{
			minor++;
			if (current->next == NULL)
			{
				return minor;
			}
			current = current->next;
			continue;
		}
		if (current->id > minor)
		{
			return minor;
		}
		current = current->next;
	}
	return minor;
}

static bool SweetSocket_isNotActiveConnection(struct SweetSocket_global_context *context)
{
	return context == NULL && context->connectionsAlive <= 0 && context->connections.base == NULL;
}

static void SweetSocket_destroyDataPool(struct SweetSocket_data_pool *data)
{
	if (data == NULL)
		return;
	if (data->next != NULL)
		SweetSocket_destroyDataPool(data->next);
	free(data);
}

struct SweetSocket_global_context *SweetSocket_initGlobalContext(enum SweetSocket_peer_type type,bool useHeader, int64_t maxConnections)
{
#ifdef WINSWEETSOCKET
	if (*((int *)(&wsaData)) == 0 && (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0))
		return 0;
#endif
	struct SweetSocket_global_context *context = (struct SweetSocket_global_context *)calloc(1, sizeof(struct SweetSocket_global_context));
	context->status = STATUS_IN_INIT;
	context->type = type;
	context->maxConnections = maxConnections;
	context->useHeader = useHeader;
	return context;
}

EXPORT bool SweetSocket_closeGlobalContext(struct SweetSocket_global_context **context)
{
	if ((*context)->connectionsAlive > 0)
	{
		SweetSocket_peerClientClose(*context, APPLY_ALL);
	}
	if ((*context)->connections.size > 0)
		SweetSocket_peerCloseSocket(*context, APPLY_ALL);
	free(*context);
	*context = NULL;
#ifdef WINSWEETSOCKET
	if (*((int *)(&wsaData)) != 0 && WSACleanup() != 0)
		return false;
#endif
	return true;
}

EXPORT int64_t SweetSocket_pushNewConnection(struct SweetSocket_peers *conn, struct SweetSocket_peer_connects *newSocket)
{
	if (conn == NULL || newSocket == NULL)
		return false;
	newSocket->id = SweetSocket_findMinorId(conn);
	conn->size++;
	if (conn->top == NULL && conn->base == NULL)
	{
		conn->top = conn->base = newSocket;
		return newSocket->id;
	}
	conn->top->next = newSocket;
	newSocket->previous = conn->top;
	conn->top = newSocket;
	return newSocket->id;
}

EXPORT bool SweetSocket_removeConnectionById(struct SweetSocket_peers *conn, uint64_t id)
{
	if (conn == NULL || conn->size == 0 || conn->top == NULL || conn->base == NULL)
		return false;
	struct SweetSocket_peer_connects *current = conn->base;
	while (current != NULL)
	{
		if (current->id == id)
		{
			if (current->previous != NULL)
			{
				current->previous->next = current->next;
			}
			if (current->next != NULL)
			{
				current->next->previous = current->previous;
			}
			if (current == conn->top)
			{
				conn->top = current->previous;
			}
			if (current == conn->base)
			{
				conn->base = current->next;
			}
			free(current);
			conn->size--;
			return true;
		}
		current = current->next;
	}
	return false;
}

EXPORT struct SweetSocket_peer_connects *SweetSocket_createPeer(struct SweetSocket_global_context *context, uint8_t type, const char *addr, uint16_t port)
{
	if (context == NULL || !(context->status > STATUS_NOT_INIT))
		return NULL;
	struct SweetSocket_peer_connects *newSocket = (struct SweetSocket_peer_connects *)calloc(1, sizeof(struct SweetSocket_peer_connects));
	newSocket->socket.type = type;
	if (addr != NULL)
	{
		newSocket->socket.addr = (char *)calloc(strlen(addr) + 1, sizeof(char));
		strcpy(newSocket->socket.addr, addr);
	}
	newSocket->socket.port = port;
	return newSocket;
}

EXPORT bool SweetSocket_peerOpenSocket(char *addr, int16_t port, struct addrinfo *hints, struct addrinfo **result, SOCKET *socketIdentifyer)
{
	char portStr[7];
	snprintf(portStr, sizeof(portStr), "%d", port);
	*socketIdentifyer = INVALID_SOCKET;
	if (getaddrinfo(addr, portStr, hints, result))
	{
		// Failed
		freeaddrinfo(*result);
		return false;
	}
	for (struct addrinfo *ptr = *result; ptr != NULL; ptr = ptr->ai_next)
	{
		*socketIdentifyer = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (*socketIdentifyer == INVALID_SOCKET)
		{
			// Failed
			continue;
		}
		*result = ptr;
		return true;
	}
	// Failed
	freeaddrinfo(*result);
	return false;
}

EXPORT bool SweetSocket_peerCloseSocket(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID)
{
	if (SweetSocket_isNotActiveConnection(context))
		return false;
	for (struct SweetSocket_peer_connects *current = context->connections.base; current != NULL;)
	{
		if (!(serverID == APPLY_ALL ? true : current->id == serverID ? true
																	 : false) ||
			current->socket.socket == 0)
		{
			current = current->next;
			continue;
		}
		closesocket(current->socket.socket);
		current->socket.socket = 0;
		while (SweetThread_isRunning(current->acceptThread))
			;
		struct SweetSocket_peer_connects *next = current->next;
		SweetSocket_removeConnectionById(&context->connections, current->id);
		current = next;
	}
	return true;
}

EXPORT bool SweetSocket_peerClientClose(struct SweetSocket_global_context *context, enum SweetSocket_apply_on clientID)
{
	struct SweetSocket_peer_clients *previous = NULL;
	for (struct SweetSocket_peer_clients *current = context->clients; current != NULL;)
	{
		if (!(clientID == APPLY_ALL ? true : current->id == clientID ? true
																	 : false))
		{
			previous = current;
			current = current->next;
			continue;
		}
		if (previous != NULL)
			previous->next = current->next;
		else
			context->clients = current->next;
		void *next = current->next;
		current->closing = true;
		if (current->client->addr != NULL)
			free(current->client->addr);
		shutdown(current->client->socket, SD_BOTH);
		closesocket(current->client->socket);
		current->client->socket = -1;
		free(current->client);
		if (current->send != NULL)
			SweetSocket_destroyDataPool(current->send);
		if (current->revice != NULL)
			SweetSocket_destroyDataPool(current->revice);
		free(current);
		current = next;
		context->connectionsAlive--;
		if (clientID != APPLY_ALL)
			break;
	}
	return true;
}

EXPORT bool SweetSocket_sendData(const char *data, uint64_t size, struct SweetSocket_global_context *context, enum SweetSocket_apply_on clientID)
{
	if (SweetSocket_isNotActiveConnection(context))
		return false;
	bool returnVal = false;
	for (struct SweetSocket_peer_clients *connection = context->clients; connection != NULL; connection = connection->next)
	{
		if (!(clientID == APPLY_ALL ? true : connection->id == clientID ? true
																		: false))
			continue;
		if (connection->sendThread.address != NULL)
		{
			struct SweetSocket_data_pool *newData = (struct SweetSocket_data_pool *)malloc(sizeof(struct SweetSocket_data_pool));
			uint64_t dataSize = 0;
			void *targetMemory = NULL;
			void *copyPoint = NULL;
			if (context->useHeader)
			{
				dataSize = sizeof(struct SweetSocket_data_header) + size;
				struct SweetSocket_data_header *header = (struct SweetSocket_data_header *)calloc(1, dataSize);
				header->size = size;
				header->command = PACKET_DATA;
				targetMemory = header;
				copyPoint = header + 1;
			}
			else
			{
				dataSize = size;
				copyPoint = malloc(size);
				targetMemory = copyPoint;
			}
			memcpy(copyPoint, data, size);
			newData->data = targetMemory;
			newData->size = dataSize;
			newData->next = NULL;
			if (connection->send == NULL)
			{
				connection->send = newData;
				continue;
			}
			struct SweetSocket_data_pool *currentData = connection->send;
			while (currentData->next != NULL)
			{
				currentData = currentData->next;
			}
			currentData->next = newData;
			returnVal = true;
			continue;
		}
		char *sendData = malloc(size);
		memcpy(sendData, data, size);
		returnVal = SweetSocket_internalSend(&sendData, size, connection->client->socket);
		free(sendData);
	}
	return returnVal;
}

EXPORT bool SweetSocket_reciveData(struct SweetSocket_global_context *context, enum SweetSocket_apply_on clientID, struct SweetSocket_data_pool *target)
{
	if (SweetSocket_isNotActiveConnection(context))
		return false;
	for (struct SweetSocket_peer_clients *connection = context->clients; connection != NULL; connection = connection->next)
	{
		if (connection->reciveThread.address != NULL && connection->revice != NULL)
		{
			*target = *(connection->revice);
			struct SweetSocket_data_pool *temp = connection->revice;
			connection->revice = connection->revice->next;
			free(temp);
			return true;
		}
	}
	return false;
}

EXPORT void SweetSocket_resolvePeer(struct SweetSocket_peer_clients *client)
{
	if (client == NULL || client->client == NULL || client->client->socket == INVALID_SOCKET || client->client->addr != NULL)
		return;
	struct sockaddr_storage localAddr;
	int addrLen = sizeof(localAddr);
	getpeername(client->client->socket, (struct sockaddr *)&localAddr, &addrLen);
	if (localAddr.ss_family == AF_INET)
	{
		struct sockaddr_in localAddr4;
		addrLen = sizeof(localAddr4);
		client->client->addr = (char *)malloc(INET_ADDRSTRLEN);
		getpeername(client->client->socket, (struct sockaddr *)&localAddr4, &addrLen);
		inet_ntop(AF_INET, (struct sockaddr *)&localAddr4.sin_addr, client->client->addr, INET_ADDRSTRLEN);
		client->client->type = AF_INET;
		client->client->port = localAddr4.sin_port;
	}
	else if (localAddr.ss_family == AF_INET6)
	{
		struct sockaddr_in6 localAddr6;
		addrLen = sizeof(localAddr6);
		client->client->addr = (char *)malloc(INET6_ADDRSTRLEN);
		getpeername(client->client->socket, (struct sockaddr *)&localAddr6, &addrLen);
		inet_ntop(AF_INET6, (struct sockaddr *)&localAddr6.sin6_addr, client->client->addr, INET6_ADDRSTRLEN);
		client->client->type = AF_INET6;
		client->client->port = localAddr6.sin6_port;
	}
}

//	Above is interal send and recive commands, this is not exported
bool SweetSocket_internalSend(char **data, uint64_t size, SOCKET id)
{
	int64_t sended = send(id, *data, size, 0);
	if (sended == 0)
	{
		// Connection closed
		closesocket(id);
		return false;
	}
	if (sended == SOCKET_ERROR)
	{
		// Failed
		closesocket(id);
		return false;
	}
	return true;
}

int64_t SweetSocket_internalRecive(char **data, uint64_t size, SOCKET id)
{
	int64_t reviced = recv(id, *data, size, 0);
	if (reviced == 0)
	{
		// Connection closed
		closesocket(id);
		free(*data);
		*data = NULL;
		return false;
	}
	if (reviced == SOCKET_ERROR)
	{
		// Failed
		closesocket(id);
		free(*data);
		data = NULL;
		return false;
	}
	return reviced;
}