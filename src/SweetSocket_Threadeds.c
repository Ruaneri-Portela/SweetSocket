#include "SweetSocket.h"

SWEETTHREAD_RETURN sendSocket(void *arg)
{
	struct intoContextSocketDataThread *sendContext = (struct intoContextSocketDataThread *)arg;
	while (sendContext->context->status == STATUS_INIT && !sendContext->connection->closing)
	{
		struct dataPool *current = sendContext->connection->send;
		while (current != NULL)
		{
			if (internalSend(current->data, current->size, sendContext->connection->client->socket))
			{
				sendContext->connection->send = current->next;
				free(current->data);
				free(current);
				current = sendContext->connection->send;
				continue;
			}
			break;
		}
		sweetThread_Sleep(10);
	}
	if (!sendContext->connection->closing)
	{
		closeClient(sendContext->context, sendContext->connection->id);
	}
	free(arg);
	SWEETTHREAD_RETURN_VALUE(1);
}

SWEETTHREAD_RETURN reciveScoket(void *arg)
{
	struct intoContextSocketDataThread *reciveContext = (struct intoContextSocketDataThread *)arg;
	char *data = NULL;
	void *basePointer = NULL;
	uint64_t size = 0;
	uint64_t permSize;
	bool isHeader = true;
	if (reciveContext->context->useHeader)
	{
		data = malloc(sizeof(struct dataHeader) + 1);
		size = sizeof(struct dataHeader);
	}
	else
	{
		data = malloc(SWEETSOCKET_BUFFER_SIZE + 1);
		size = SWEETSOCKET_BUFFER_SIZE;
	}
	permSize = size;
	while (true)
	{
		memset(data, 0, size + 1);
		int64_t recived = internalRecv(data, size, reciveContext->connection->client->socket);
		if (reciveContext->context->status != STATUS_INIT || recived == 0)
			break;
		if (reciveContext->context->useHeader)
		{
			if (isHeader)
			{
				struct dataHeader *header = (struct dataHeader *)data;
				data = malloc(header->size + 1);
				size = header->size;
				free(header);
				isHeader = false;
			}
			continue;
		}
		if (basePointer == NULL && recived == size)
		{
			u_long toRevice;
			ioctlsocket(reciveContext->connection->client->socket, FIONREAD, &toRevice);
			if (toRevice > 0)
			{
				basePointer = realloc(data, size + toRevice + 1);
				data = (char *)basePointer + size;
				size = toRevice;
			}
			continue;
		}
		if (reciveContext->function != NULL)
		{
			void *ptr = (basePointer == NULL ? data : basePointer);
			uint64_t dataSize = (basePointer == NULL ? recived : (reciveContext->context->useHeader ? recived : recived + permSize));
			reciveContext->function(ptr, dataSize, reciveContext->context, reciveContext->connection, reciveContext->intoExternaParm);
		}
		else
		{
			struct dataPool *newNode = (struct dataPool *)malloc(sizeof(struct dataPool));
			newNode->data = (basePointer == NULL ? data : basePointer);
			newNode->size = size;
			newNode->next = NULL;
			if (reciveContext->connection->revice == NULL)
			{
				reciveContext->connection->revice = newNode;
			}
			else
			{
				struct dataPool *current = reciveContext->connection->revice;
				while (current->next != NULL)
				{
					current = current->next;
				}
				current->next = newNode;
			}
		}
		data = malloc(permSize + 1);
		size = permSize;
		isHeader = true;
		basePointer = NULL;
	}
	if (!reciveContext->connection->closing)
	{
		closeClient(reciveContext->context, reciveContext->connection->id);
	}
	free(arg);
	SWEETTHREAD_RETURN_VALUE(1);
}

SWEETTHREAD_RETURN acceptSocket(void *arg)
{
	struct acceptIntoContextSocket *acceptContext = (struct acceptIntoContextSocket *)arg;
	while (true)
	{
		struct socketData *client = (struct socketData *)calloc(1, sizeof(struct socketData));
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
		struct socketClients *newClient = (struct socketClients *)calloc(1, sizeof(struct socketClients));
		newClient->client = client;
		newClient->id = acceptContext->context->minClientID++;
		if (acceptContext->connection->enableRecivePool)
		{
			// Start recive thread
			struct intoContextSocketDataThread *reciveContext = (struct intoContextSocketDataThread *)calloc(1, sizeof(struct intoContextSocketDataThread));
			reciveContext->connection = newClient;
			reciveContext->context = acceptContext->context;
			reciveContext->function = acceptContext->functionRecv;
			reciveContext->intoExternaParm = acceptContext->intoExternaParmRecv;
			reciveContext->connection->reciveThread = sweetThread_CreateThread(reciveScoket, (void *)reciveContext, true);
		}
		if (acceptContext->connection->enableSendPool)
		{
			// Start send thread
			struct intoContextSocketDataThread *sendContext = (struct intoContextSocketDataThread *)calloc(1, sizeof(struct intoContextSocketDataThread));
			sendContext->connection = newClient;
			sendContext->context = acceptContext->context;
			sendContext->function = acceptContext->functionSend;
			sendContext->intoExternaParm = acceptContext->intoExternaParmSend;
			sendContext->connection->sendThread = sweetThread_CreateThread(sendSocket, (void *)sendContext, true);
		}
		//
		bool findMinorId = false;
		if (acceptContext->context->clients == NULL)
		{
			acceptContext->context->clients = newClient;
			continue;
		}
		for (struct socketClients *current = acceptContext->context->clients; current != NULL; current = current->next)
		{
			if (current->next == NULL)
			{
				current->next = newClient;
				break;
			}
		}
	}
	free(arg);
	SWEETTHREAD_RETURN_VALUE(1);
}