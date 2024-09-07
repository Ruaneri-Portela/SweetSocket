#include "SweetSocket.h"

SWEETTHREAD_RETURN SweetSocket_sendThread(void *arg)
{
	struct SweetSocket_data_context_thread *sendContext = (struct SweetSocket_data_context_thread *)arg;
	while (sendContext->context->status == STATUS_INIT && !sendContext->connection->closing)
	{
		struct SweetSocket_data_pool *current = sendContext->connection->send;
		while (current != NULL)
		{
			if (SweetSocket_internalSend(&(current->data), current->size, sendContext->connection->client->socket))
			{
				sendContext->connection->send = current->next;
				free(current->data);
				free(current);
				current = sendContext->connection->send;
				continue;
			}
			break;
		}
		SweetThread_sleep(10);
	}
	if (!sendContext->connection->closing)
	{
		SweetSocket_peerClientClose(sendContext->context, sendContext->connection->id);
	}
	free(arg);
	SWEETTHREAD_RETURN_VALUE(1);
}

SWEETTHREAD_RETURN SweetSocket_reciveThread(void *arg)
{
	struct SweetSocket_data_context_thread *reciveContext = (struct SweetSocket_data_context_thread *)arg;
	char *data = NULL;
	void *basePointer = NULL;
	uint64_t size = 0;
	uint64_t permSize;
	bool isHeader = true;
	if (reciveContext->context->useHeader)
	{
		data = malloc(sizeof(struct SweetSocket_data_header) + 1);
		size = sizeof(struct SweetSocket_data_header);
	}
	else
	{
		data = malloc(SWEETSOCKET_BUFFER_SIZE + 1);
		size = SWEETSOCKET_BUFFER_SIZE;
	}
	permSize = size;
	while (reciveContext->context->status == STATUS_INIT && (reciveContext->connection->closing == 0) && (reciveContext->connection->client != NULL))
	{
		int64_t recived = SweetSocket_internalRecive(&data, size, reciveContext->connection->client->socket);
		if (reciveContext->context->status != STATUS_INIT || recived == 0 || reciveContext->connection->closing)
		{
			data = NULL;
			break;
		}
		if (reciveContext->context->useHeader)
		{
			if (isHeader)
			{
				struct SweetSocket_data_header *header = (struct SweetSocket_data_header *)data;
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
			enum SweetSocket_sweet_callback_status status = reciveContext->function(ptr, dataSize, reciveContext->context, reciveContext->connection, reciveContext->intoExternaParm);
			free(ptr);
			data = NULL;
			if (status == SWEET_SOCKET_CALLBACK_CLOSE || status == SWEET_SOCKET_CALLBACK_ERROR)
				break;
		}
		else
		{
			struct SweetSocket_data_pool *newNode = (struct SweetSocket_data_pool *)malloc(sizeof(struct SweetSocket_data_pool));
			newNode->data = (basePointer == NULL ? data : basePointer);
			newNode->size = size;
			newNode->next = NULL;
			if (reciveContext->connection->revice == NULL)
			{
				reciveContext->connection->revice = newNode;
			}
			else
			{
				struct SweetSocket_data_pool *current = reciveContext->connection->revice;
				while (current->next != NULL)
				{
					current = current->next;
				}
				current->next = newNode;
			}
		}
		data = calloc(1, permSize + 1);
		size = permSize;
		isHeader = true;
		basePointer = NULL;
	}
	if (!reciveContext->connection->closing)
		SweetSocket_peerClientClose(reciveContext->context, reciveContext->connection->id);
	if (data != NULL)
		free(data);
	free(arg);
	SWEETTHREAD_RETURN_VALUE(1);
}

SWEETTHREAD_RETURN SweetSocket_acceptConnectionThread(void *arg)
{
	struct SweetSocket_accept_data_context_thread *acceptContext = (struct SweetSocket_accept_data_context_thread *)arg;
	while (true)
	{
		struct SweetSocket_peer_data *client = (struct SweetSocket_peer_data *)calloc(1, sizeof(struct SweetSocket_peer_data));
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
		if (acceptContext->context->connectionsAlive >= acceptContext->context->maxConnections && !(acceptContext->context->maxConnections < 0))
		{
			// Send ACk to notificate that the server is full
			closesocket(client->socket);
			free(client);
			continue;
		}
		// Send ACK to notificate that the connection was accepted
		acceptContext->context->connectionsAlive++;
		struct SweetSocket_peer_clients *newClient = (struct SweetSocket_peer_clients *)calloc(1, sizeof(struct SweetSocket_peer_clients));
		newClient->client = client;
		newClient->id = acceptContext->context->minClientID++;
		if (acceptContext->connection->enableRecivePool)
		{
			// Start recive thread
			struct SweetSocket_data_context_thread *reciveContext = (struct SweetSocket_data_context_thread *)calloc(1, sizeof(struct SweetSocket_data_context_thread));
			reciveContext->connection = newClient;
			reciveContext->context = acceptContext->context;
			reciveContext->function = acceptContext->functionRecv;
			reciveContext->intoExternaParm = acceptContext->intoExternaParmRecv;
			reciveContext->connection->reciveThread = SweetThread_createThread(SweetSocket_reciveThread, (void *)reciveContext, true);
		}
		if (acceptContext->connection->enableSendPool)
		{
			// Start send thread
			struct SweetSocket_data_context_thread *sendContext = (struct SweetSocket_data_context_thread *)calloc(1, sizeof(struct SweetSocket_data_context_thread));
			sendContext->connection = newClient;
			sendContext->context = acceptContext->context;
			sendContext->function = acceptContext->functionSend;
			sendContext->intoExternaParm = acceptContext->intoExternaParmSend;
			sendContext->connection->sendThread = SweetThread_createThread(SweetSocket_sendThread, (void *)sendContext, true);
		}
		//
		if (acceptContext->context->clients == NULL)
		{
			acceptContext->context->clients = newClient;
			continue;
		}
		for (struct SweetSocket_peer_clients *current = acceptContext->context->clients; current != NULL; current = current->next)
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