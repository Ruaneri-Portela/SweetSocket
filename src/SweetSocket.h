#if defined(_WIN32) || defined(_WIN64)
#define _CRT_SECURE_NO_WARINIGS
#define WINSWEETSOCKET
#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#define EXPORT __declspec(dllexport)
#pragma comment(lib, "ws2_32.lib")
#endif

#include "SweetThread/SweetThread.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

enum poolBehaviour {
	NO_POOL = 0,
	ONLY_RECIVE,
	ONLY_SEND,
	BOTH
};

struct socketData
{
	uint8_t type;
	char* addr;
	uint16_t port;
	uint64_t socket;
};

struct sockets
{
	struct socketConnection* top;
	struct socketConnection* base;
	uint64_t size;
};

struct dataHeader
{
	enum packetType command;
	uint64_t size;
};

struct dataPool
{
	char* data;
	uint64_t size;
	struct dataPool* next;
};

struct socketClients
{
	struct socketData* client;
	struct threadIdentifyer reciveThread;
	struct threadIdentifyer sendThread;
	struct dataPool* revice;
	struct dataPool* send;
	uint64_t id;
	bool closing;
	struct socketClients* next;
};

struct socketConnection
{
	uint64_t id;
	struct socketData socket;
	struct threadIdentifyer acceptThread;
	struct socketConnection* next;
	struct socketConnection* previous;
	bool enableRecivePool;
	bool enableSendPool;
};

struct socketGlobalContext
{
	struct sockets connections;
	enum statusCode status;
	enum socketType type;
	int64_t connectionsAlive;
	int64_t maxConnections;
	int64_t minClientID;
	struct socketClients* clients;
	bool useHeader;
};

struct acceptIntoContextSocket
{
	struct socketGlobalContext* context;
	struct socketConnection* connection;
	void (*functionSend)(void*, uint64_t, struct socketGlobalContext*, struct socketClients*, void*);
	void* intoExternaParmRecv;
	void (*functionRecv)(void*, uint64_t, struct socketGlobalContext*, struct socketClients*, void*);
	void* intoExternaParmSend;
};

struct intoContextSocketDataThread
{
	struct socketGlobalContext* context;
	struct socketClients* connection;
	void (*function)(void*, uint64_t, struct socketGlobalContext*, struct socketClients*, void*);
	void* intoExternaParm;
};

EXPORT struct socketGlobalContext* initSocketGlobalContext(enum socketType type);

EXPORT bool closeSocketGlobalContext(struct socketGlobalContext** context);

EXPORT int64_t pushNewConnection(struct sockets* conn, struct socketConnection* newSocket);

EXPORT bool removeConnectionById(struct sockets* conn, uint64_t id);

EXPORT struct socketConnection* createSocket(struct socketGlobalContext* context, uint8_t type, const char* addr, uint16_t port);

EXPORT bool openSocket(char* addr, int16_t port, struct addrinfo* hints, struct addrinfo** result, SOCKET* socketIdentifyer);

EXPORT bool closeSocket(struct socketGlobalContext* context, enum applyOn serverID);

EXPORT bool closeClient(struct socketGlobalContext* context, enum applyOn clientID);

EXPORT bool sendData(void *data, uint64_t size, struct socketGlobalContext *context, enum applyOn clientID);

EXPORT bool reviceData(struct socketGlobalContext* context, enum applyOn clientID, struct dataPool* target);


EXPORT bool startConnection(struct socketGlobalContext* context, enum applyOn serverID);

EXPORT bool enablePools(struct socketGlobalContext* context, enum applyOn clientID, bool enableRecivePool, bool enableSendPool);


EXPORT bool startAccepting(struct socketGlobalContext* context, enum applyOn serverID, void* functionSend, void* functionRecv, void* parmsRecv, void* parmsSend, enum poolBehaviour pool);

EXPORT bool startListening(struct socketGlobalContext* context, enum applyOn serverID);

//	Above is interal send and recive commands, this is not exported
bool internalSend(void* data, uint64_t size, SOCKET id);

int64_t internalRecv(void* data, uint64_t size, SOCKET id);
