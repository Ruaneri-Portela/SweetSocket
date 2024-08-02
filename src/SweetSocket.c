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

bool closeSocketGlobalContext(struct socketGlobalContext **context)
{
    if ((*context)->connections.size > 0)
    {
        closeSocket(*context, APPLY_ALL);
    }
    free(*context);
    *context = NULL;
#ifdef WINSWEETSOCKET
    if (*((int *)(&wsaData)) != 0 && WSACleanup() != 0)
        return false;
#endif
    return true;
}

int64_t pushNewConnection(struct sockets *conn, struct socketConnection *newSocket)
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

bool removeConnectionById(struct sockets *conn, uint64_t id)
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

struct socketConnection *createSocket(struct socketGlobalContext *context, uint8_t type, const char *addr, uint16_t port)
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

bool openSocket(char *addr, int16_t port, struct addrinfo *hints, struct addrinfo **result, SOCKET *socketIdentifyer)
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

bool closeSocket(struct socketGlobalContext *context, enum applyOn serverID)
{
    if (isNotActiveConnection(context))
        return false;
    for (struct socketConnection *current = context->connections.base; current != NULL; current = current->next)
    {
        if (!(serverID == APPLY_ALL ? true : current->id == serverID ? true
                                                                     : false) ||
            current->socket.socket == 0)
            continue;
        closesocket(current->socket.socket);
        current->socket.socket = 0;
        if (context->type == SOCKET_SERVER)
        {
            while (current->clients != NULL)
            {
                closesocket(current->clients->client->socket);
                struct socketClients *next = current->clients->next;
                while (sweetThread_IsRunning(current->clients->reciveThread))
                    ;
                free(current->clients);
                current->clients = next;
            }
            while (sweetThread_IsRunning(current->acceptThread))
                ;
        }
        removeConnectionById(&context->connections, current->id);
    }
    return true;
}

void sendData(void *data, uint64_t size, struct socketGlobalContext *context, enum applyOn connectionID, enum applyOn clientID)
{
    if (isNotActiveConnection(context))
        return;
    for (struct socketConnection *current = context->connections.base; current != NULL; current = current->next)
    {
        if (!(connectionID == APPLY_ALL ? true : current->id == connectionID ? true
                                                                             : false) ||
            current->clients == NULL)
            continue;
        for (struct socketClients *connection = current->clients; connection != NULL; connection = connection->next)
        {
            if (!(clientID == APPLY_ALL ? true : connection->id == clientID ? true
                                                                            : false))
                continue;
            if (connection->sendThread.address != NULL)
            {
                struct dataPool* newData = (struct dataPool*)malloc(sizeof(struct dataPool));
                uint64_t dataSize = 0;
                void* targetMemory = NULL;
                void* copyPoint = NULL;
                if(context->useHeader){
                    dataSize = sizeof(struct dataHeader) + size;
                    struct dataHeader* header = (struct dataHeader*)calloc(1, dataSize);
                    header->size = size;
                    header->command = PACKET_DATA;
                    targetMemory = header;
                    copyPoint = header + 1;
                }
                else {
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
                continue;
            }
        }
    }
}

bool reviceData(struct socketGlobalContext *context, enum applyOn connectionID, enum applyOn clientID, struct dataPool *target)
{
    if (isNotActiveConnection(context) || connectionID == APPLY_ALL)
        return false;
    for (struct socketConnection *current = context->connections.base; current != NULL; current = current->next)
    {
        if (!(current->id == connectionID))
            continue;
        for (struct socketClients *connection = current->clients; connection != NULL; connection = connection->next)
        {
            if (connection->reciveThread.address != NULL)
            {
                if (connection->revice == NULL)
                    continue;
                *target = *(connection->revice);
                struct dataPool *temp = connection->revice;
                connection->revice = connection->revice->next;
                free(temp);
                return true;
            }
        }
    }
    return false;
}