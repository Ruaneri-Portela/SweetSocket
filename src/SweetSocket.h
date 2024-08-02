#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "SweetThread/SweetThread.h"

#if defined(_WIN32) || defined(_WIN64)
#define _CRT_SECURE_NO_WARINIGS
#define WINSWEETSOCKET
#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#define SWEETSOCKET_BUFFER_SIZE 512

enum statusCode
{
    STATUS_NOT_INIT = 0,
    STATUS_IN_INIT,
    STATUS_INIT,
    STATUS_IN_CLOSE,
    STATUS_CLOSED
};

enum socketType
{
    SOCKET_NULL = 0,
    SOCKET_SERVER,
    SOCKET_CLIENT
};

enum applyOn
{
    APPLY_ALL = -1,
};

enum packetType
{
    PACKET_NULL = 0,
    PACKET_DATA,
    PACKET_PING,
    PACKET_PONG,
    PACKET_CLOSE
};

struct socketData
{
    uint8_t type;
    char *addr;
    uint16_t port;
    uint64_t socket;
};

struct sockets
{
    struct socketConnection *top;
    struct socketConnection *base;
    uint64_t size;
};

struct dataHeader
{
    enum packetType command;
    uint64_t size;
};

struct dataPool
{
    char *data;
    uint64_t size;
    struct dataPool *next;
};

struct socketClients
{
    struct socketData *client;
    struct threadIdentifyer reciveThread;
    struct threadIdentifyer sendThread;
    struct dataPool *revice;
    struct dataPool *send;
    uint64_t id;
    bool closing;
    struct socketClients *next;
};

struct socketConnection
{
    struct socketData socket;
    struct socketConnection *next;
    struct socketConnection *previous;
    uint64_t id;
    struct threadIdentifyer acceptThread;
    struct socketClients *clients;
    bool enableRecivePool;
    bool enableSendPool;
};

struct socketGlobalContext
{
    struct sockets connections;
    enum statusCode status;
    enum socketType type;
    uint64_t connectionsAlive;
    int64_t maxConnections;
    bool useHeader;
};

struct acceptIntoContextSocket
{
    struct socketGlobalContext *context;
    struct socketConnection *connection;
};

struct intoContextSocketDataThread
{
    struct socketGlobalContext *context;
    struct socketClients *connection;
};

struct socketGlobalContext *initSocketGlobalContext(enum socketType type);

bool closeSocketGlobalContext(struct socketGlobalContext **context);

int64_t pushNewConnection(struct sockets *conn, struct socketConnection *newSocket);

bool removeConnectionById(struct sockets *conn, uint64_t id);

struct socketConnection *createSocket(struct socketGlobalContext *context, uint8_t type, const char *addr, uint16_t port);

bool openSocket(char *addr, int16_t port, struct addrinfo *hints, struct addrinfo **result, SOCKET *socketIdentifyer);

bool closeSocket(struct socketGlobalContext *context, enum applyOn serverID);

void sendData(void *data, uint64_t size, struct socketGlobalContext *context, enum applyOn connectionID, enum applyOn clientID);

bool reviceData(struct socketGlobalContext *context, enum applyOn connectionID, enum applyOn clientID, struct dataPool *target);

bool enablePools(struct socketGlobalContext *context, enum applyOn serverID, bool enableRecivePool, bool enableSendPool);