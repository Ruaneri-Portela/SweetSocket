#include "SweetSocket.h"
#include "SweetSocket_Threadeds.h"

static bool invalidAction(struct socketGlobalContext *context)
{
    return context == NULL || context->type == SOCKET_SERVER || context->status == STATUS_NOT_INIT || context->status == STATUS_CLOSED || context->status == STATUS_IN_CLOSE || context->connections.size <= 0 || context->connections.base == NULL;
    ;
}

bool startConnection(struct socketGlobalContext *context, enum applyOn serverID)
{
    if (invalidAction(context))
        return false;
    for (struct socketConnection *current = context->connections.base; current != NULL; current = current->next)
    {
        if (!(serverID == APPLY_ALL ? true : current->id == serverID ? true
                                                                     : false))
            continue;
        struct addrinfo hints = {0};
        hints.ai_family = current->socket.type;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        struct addrinfo *result = NULL;
        if (!openSocket(current->socket.addr, current->socket.port, &hints, &result, &current->socket.socket))
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
};

bool stopConnection(struct socketGlobalContext *context, enum applyOn serverID);

bool enablePools(struct socketGlobalContext *context, enum applyOn serverID, bool enableRecivePool, bool enableSendPool)
{
    if (invalidAction(context))
        return false;
    for (struct socketConnection *current = context->connections.base; current != NULL; current = current->next)
    {
        if (!(serverID == APPLY_ALL ? true : current->id == serverID ? true
                                                                     : false) ||
            (current->enableRecivePool && current->enableSendPool))
            continue;
        current->enableRecivePool = current->enableRecivePool ? true : enableRecivePool;
        current->enableSendPool = current->enableSendPool ? true : enableSendPool;
        if (current->clients == NULL)
        {
            current->clients = calloc(1, sizeof(struct socketClients));
            current->clients->client = &current->socket;
            current->clients->id = current->id;
        }
        if (enableRecivePool)
        {
            struct intoContextSocketDataThread *reciveContext = (struct intoContextSocketDataThread *)calloc(1, sizeof(struct intoContextSocketDataThread));
            reciveContext->context = context;
            reciveContext->connection = current->clients;
            reciveContext->connection->reciveThread = sweetThread_CreateThread(reciveScoket, reciveContext, true);
        }
        if (enableSendPool)
        {
            struct intoContextSocketDataThread *sendContext = (struct intoContextSocketDataThread *)calloc(1, sizeof(struct intoContextSocketDataThread));
            sendContext->context = context;
            sendContext->connection = current->clients;
            sendContext->connection->sendThread = sweetThread_CreateThread(sendSocket, sendContext, true);
        }
    }
    return true;
}