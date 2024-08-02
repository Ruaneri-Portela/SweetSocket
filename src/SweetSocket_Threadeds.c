#include "SweetSocket.h"

SWEETTHREAD_RETURN sendSocket(void* arg)
{
	struct intoContextSocketDataThread* sendContext = (struct intoContextSocketDataThread*)arg;
	while (sendContext->context->status == STATUS_INIT && !sendContext->connection->closing)
	{
		struct dataPool* current = sendContext->connection->send;
		while (current != NULL)
		{
			int32_t sended = send(sendContext->connection->client->socket, current->data, current->size, 0);
			sendContext->connection->send = current->next;
			free(current->data);
			free(current);
			if (sended == 0)
			{
				// Connection closed
				closesocket(sendContext->connection->client->socket);
				break;
			}
			if (sended == SOCKET_ERROR)
			{
				// Failed
				closesocket(sendContext->connection->client->socket);
				break;
			}
			current = sendContext->connection->send;
		}
		sweetThread_Sleep(10);
	}
	if (sendContext->connection->closing == false)
	{
		sendContext->context->connectionsAlive--;
		sendContext->connection->closing = true;
	}
	SWEETTHREAD_RETURN_VALUE(1);
}

SWEETTHREAD_RETURN reciveScoket(void* arg)
{
	struct intoContextSocketDataThread* reciveContext = (struct intoContextSocketDataThread*)arg;
	void* data = NULL;
	void* basePointer = NULL;
	uint64_t size = NULL;
	uint64_t permSize;
	bool isHeader = true;
	if (reciveContext->context->useHeader) {
		data = malloc(sizeof(struct dataHeader));
		size = sizeof(struct dataHeader);
	}
	else {
		data = malloc(SWEETSOCKET_BUFFER_SIZE);
		size = SWEETSOCKET_BUFFER_SIZE;
	}
	permSize = size;
	while (true)
	{
		int32_t reviced = recv(reciveContext->connection->client->socket, data, size, 0);
		if (reciveContext->context->status != STATUS_INIT)
		{
			// Procedure to close the server
			free(data);
			break;
		}
		if (reviced == 0)
		{
			// Connection closed
			closesocket(reciveContext->connection->client->socket);
			free(data);
			break;
		}
		if (reviced == SOCKET_ERROR)
		{
			// Failed
			closesocket(reciveContext->connection->client->socket);
			free(data);
			break;
		}
		if (reciveContext->context->useHeader) {
			if (isHeader)
			{
				struct dataHeader* header = (struct dataHeader*)data;
				data = malloc(header->size);
				size = header->size;
				free(header);
				isHeader = false;
			}
			continue;
		}
		if (basePointer == NULL && reviced == size) {
			uint32_t toRevice;
			ioctlsocket(reciveContext->connection->client->socket, FIONREAD, &toRevice);
			if (toRevice > 0) {
				basePointer = realloc(data, size + toRevice);
				data = (char*)basePointer + size;
				size = toRevice;
			}
			continue;
		}
		struct dataPool* newNode = (struct dataPool*)malloc(sizeof(struct dataPool));
		newNode->data = (basePointer == NULL ? data : basePointer);
		newNode->size = size;
		newNode->next = NULL;
		if (reciveContext->connection->revice == NULL)
		{
			reciveContext->connection->revice = newNode;
		}
		else
		{
			struct dataPool* current = reciveContext->connection->revice;
			while (current->next != NULL)
			{
				current = current->next;
			}
			current->next = newNode;
		}
		data = malloc(permSize);
		size = permSize;
		isHeader = true;
		basePointer = NULL;
	}
	if (reciveContext->connection->closing == false)
	{
		reciveContext->context->connectionsAlive--;
		reciveContext->connection->closing = true;
	}
	SWEETTHREAD_RETURN_VALUE(1);
}

SWEETTHREAD_RETURN acceptSocket(void* arg)
{
	struct acceptIntoContextSocket* acceptContext = (struct acceptIntoContextSocket*)arg;
	while (true)
	{
		struct socketData* client = (struct socketData*)calloc(1, sizeof(struct socketData));
		client->socket = accept(acceptContext->connection->socket.socket, NULL, NULL);
		if (acceptContext->context->status != STATUS_INIT || acceptContext->connection->socket.socket == 0)
		{
			// Procedure to close the server
			free(client);
			break;
		}
		if (client->socket == INVALID_SOCKET)
		{
			// Failed
			free(client);
			continue;
		}
		if (acceptContext->context->connectionsAlive > acceptContext->context->maxConnections)
		{
			// Send ACk to notificate that the server is full
			closesocket(client->socket);
			free(client);
			continue;
		}
		// Send ACK to notificate that the connection was accepted
		acceptContext->context->connectionsAlive++;
		struct socketClients* newClient = (struct socketClients*)calloc(1, sizeof(struct socketClients));
		newClient->client = client;
		if (acceptContext->connection->enableRecivePool)
		{
			// Start recive thread
			struct intoContextSocketDataThread* reciveContext = (struct intoContextSocketDataThread*)calloc(1, sizeof(struct intoContextSocketDataThread));
			reciveContext->connection = newClient;
			reciveContext->context = acceptContext->context;
			reciveContext->connection->reciveThread = sweetThread_CreateThread(reciveScoket, reciveContext, true);
		}
		if (acceptContext->connection->enableSendPool)
		{
			// Start send thread
			struct intoContextSocketDataThread* sendContext = (struct intoContextSocketDataThread*)calloc(1, sizeof(struct intoContextSocketDataThread));
			sendContext->connection = newClient;
			sendContext->context = acceptContext->context;
			sendContext->connection->sendThread = sweetThread_CreateThread(sendSocket, sendContext, true);
		}
		//
		bool findMinorId = false;
		if (acceptContext->connection->clients == NULL)
		{
			acceptContext->connection->clients = newClient;
			continue;
		}
		for (struct socketClients* current = acceptContext->connection->clients; current != NULL; current = current->next)
		{
			if (current->next == NULL)
			{
				current->next = newClient;
				break;
			}
		}
	}
	SWEETTHREAD_RETURN_VALUE(1);
}