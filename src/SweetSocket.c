#include "SweetSocket.h"
#include "SweetSocket_Threadeds.h"
#include <stdio.h>

#ifdef WINSWEETSOCKET
WSADATA wsaData = {0};
#endif

static uint64_t findMinorId(struct sockets *conn)
{
	uint64_t minor = 1;
	if (conn == NULL || conn->size == 0 || conn->base == NULL)
		return minor;
	struct socketConnection *current = conn->base;
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

static bool isNotActiveConnection(struct socketGlobalContext *context)
{
	return context == NULL && context->connectionsAlive <= 0 && context->connections.base == NULL;
};

static void destroyDataPool(struct dataPool *data)
{
	if (data == NULL)
		return;
	if (data->next != NULL)
		destroyDataPool(data->next);
	free(data);
}

struct socketGlobalContext *initSocketGlobalContext(enum socketType type)
{
#ifdef WINSWEETSOCKET
	if (*((int *)(&wsaData)) == 0 && (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0))
		return 0;
#endif
	struct socketGlobalContext *context = (struct socketGlobalContext *)calloc(1, sizeof(struct socketGlobalContext));
	context->status = STATUS_IN_INIT;
	context->type = type;
	context->maxConnections = INT64_MAX;
	return context;
}

EXPORT bool closeSocketGlobalContext(struct socketGlobalContext **context)
{
	if ((*context)->connectionsAlive > 0)
	{
		closeClient(*context, APPLY_ALL);
	}
	if ((*context)->connections.size > 0)
		closeSocket(*context, APPLY_ALL);
	free(*context);
	*context = NULL;
#ifdef WINSWEETSOCKET
	if (*((int *)(&wsaData)) != 0 && WSACleanup() != 0)
		return false;
#endif
	return true;
}

EXPORT int64_t pushNewConnection(struct sockets *conn, struct socketConnection *newSocket)
{
	if (conn == NULL || newSocket == NULL)
		return false;
	newSocket->id = findMinorId(conn);
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

EXPORT bool removeConnectionById(struct sockets *conn, uint64_t id)
{
	if (conn == NULL || conn->size == 0 || conn->top == NULL || conn->base == NULL)
		return false;
	struct socketConnection *current = conn->base;
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

EXPORT struct socketConnection *createSocket(struct socketGlobalContext *context, uint8_t type, const char *addr, uint16_t port)
{
	if (context == NULL || !(context->status > STATUS_NOT_INIT))
		return NULL;
	struct socketConnection *newSocket = (struct socketConnection *)calloc(1, sizeof(struct socketConnection));
	newSocket->socket.type = type;
	if (addr != NULL)
	{
		newSocket->socket.addr = (char *)calloc(strlen(addr) + 1, sizeof(char));
		strcpy(newSocket->socket.addr, addr);
	}
	newSocket->socket.port = port;
	return newSocket;
}

EXPORT bool openSocket(char *addr, int16_t port, struct addrinfo *hints, struct addrinfo **result, SOCKET *socketIdentifyer)
{
	char portStr[6];
	snprintf(portStr, sizeof(portStr), "%d", port);
	*socketIdentifyer = INVALID_SOCKET;
	if (getaddrinfo(addr, portStr, hints, result))
	{
		// Failed
		freeaddrinfo(*result);
		return false;
	}
	for (struct addrinfo *ptr = *result; ptr != NULL; ptr->ai_next)
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

EXPORT bool closeSocket(struct socketGlobalContext *context, enum applyOn serverID)
{
	if (isNotActiveConnection(context))
		return false;
	for (struct socketConnection *current = context->connections.base; current != NULL;)
	{
		if (!(serverID == APPLY_ALL ? true : current->id == serverID ? true
																	 : false) ||
			current->socket.socket == 0)
			continue;
		closesocket(current->socket.socket);
		current->socket.socket = 0;
		while (sweetThread_IsRunning(current->acceptThread))
			;
		struct socketConnection *next = current->next;
		removeConnectionById(&context->connections, current->id);
		current = next;
	}
	return true;
}

EXPORT bool closeClient(struct socketGlobalContext *context, enum applyOn clientID)
{
	struct socketClients *previous = NULL;
	for (struct socketClients *current = context->clients; current != NULL;)
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
			destroyDataPool(current->send);
		if (current->revice != NULL)
			destroyDataPool(current->revice);
		free(current);
		current = next;
		context->connectionsAlive--;
		if (clientID != APPLY_ALL)
			break;
	}
	return true;
}

EXPORT bool sendData(const char *data, uint64_t size, struct socketGlobalContext *context, enum applyOn clientID)
{
	if (isNotActiveConnection(context))
		return false;
	bool returnVal = false;
	for (struct socketClients *connection = context->clients; connection != NULL; connection = connection->next)
	{
		if (!(clientID == APPLY_ALL ? true : connection->id == clientID ? true
																		: false))
			continue;
		if (connection->sendThread.address != NULL)
		{
			struct dataPool *newData = (struct dataPool *)malloc(sizeof(struct dataPool));
			uint64_t dataSize = 0;
			void *targetMemory = NULL;
			void *copyPoint = NULL;
			if (context->useHeader)
			{
				dataSize = sizeof(struct dataHeader) + size;
				struct dataHeader *header = (struct dataHeader *)calloc(1, dataSize);
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
			struct dataPool *currentData = connection->send;
			while (currentData->next != NULL)
			{
				currentData = currentData->next;
			}
			currentData->next = newData;
			returnVal = true;
			continue;
		}
		void *sendData = malloc(size);
		memcpy(sendData, data, size);
		returnVal = internalSend(sendData, size, connection->client->socket);
		free(sendData);
	}
	return returnVal;
}

EXPORT bool reviceData(struct socketGlobalContext *context, enum applyOn clientID, struct dataPool *target)
{
	if (isNotActiveConnection(context))
		return false;
	for (struct socketClients *connection = context->clients; connection != NULL; connection = connection->next)
	{
		if (connection->reciveThread.address != NULL && connection->revice != NULL)
		{
			*target = *(connection->revice);
			struct dataPool *temp = connection->revice;
			connection->revice = connection->revice->next;
			free(temp);
			return true;
		}
	}
	return false;
}

EXPORT void resolvePeer(struct socketClients *client)
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
bool internalSend(const char *data, uint64_t size, SOCKET id)
{
	int64_t sended = send(id, data, size, 0);
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

int64_t internalRecv(void *data, uint64_t size, SOCKET id)
{
	int64_t reviced = recv(id, data, size, 0);
	if (reviced == 0)
	{
		// Connection closed
		closesocket(id);
		free(data);
		return false;
	}
	if (reviced == SOCKET_ERROR)
	{
		// Failed
		closesocket(id);
		free(data);
		return false;
	}
	return reviced;
}