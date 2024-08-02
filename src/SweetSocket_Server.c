#include "SweetSocket.h"
#include "SweetSocket_Threadeds.h"

static bool invalidAction(struct socketGlobalContext *context)
{
    return context == NULL || context->type == SOCKET_CLIENT || context->status == STATUS_NOT_INIT || context->status == STATUS_CLOSED || context->status == STATUS_IN_CLOSE || context->connections.size <= 0 || context->connections.base == NULL;
    ;
}

bool startAccepting(struct socketGlobalContext *context, enum applyOn serverID)
{
    if (invalidAction(context))
        return false;
    for (struct socketConnection *current = context->connections.base; current != NULL; current = current->next)
    {
        if (!(serverID == APPLY_ALL ? true : current->id == serverID ? true
                                                                     : false) ||
            current->socket.socket == 0)
            continue;
        struct acceptIntoContextSocket *acceptContext = (struct acceptIntoContextSocket *)calloc(1, sizeof(struct acceptIntoContextSocket));
        acceptContext->context = context;
        acceptContext->connection = current;
        acceptContext->connection->enableRecivePool = true;
        acceptContext->connection->enableSendPool = true;
        acceptContext->context->status = STATUS_INIT;
        acceptContext->connection->acceptThread = sweetThread_CreateThread(acceptSocket, acceptContext, true);
    }
    return true;
}

bool startListening(struct socketGlobalContext *context, enum applyOn serverID)
{
    if (context == NULL || !(context->type == SOCKET_SERVER) || context->connections.size == 0)
        return false;
    for (struct socketConnection *current = context->connections.base; current != NULL; current = current->next)
    {
        if (!(current->socket.type == AF_INET ^ current->socket.type == AF_INET6) ||
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
        if (!openSocket(current->socket.addr, current->socket.port, &hints, &result, &current->socket.socket))
        {
            // Failed
            continue;
        }
        if (bind(current->socket.socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
        {
            // Failed
            freeaddrinfo(result);
            continue;
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
