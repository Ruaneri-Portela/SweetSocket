#if defined(_WIN32) || defined(_WIN64)
#define WINSWEETSOCKET
#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#define EXPORT __declspec(dllexport)
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#endif

#include "SweetThread/SweetThread.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SWEETSOCKET_BUFFER_SIZE 512

enum SweetSocket_sweet_status_code
{
	STATUS_NOT_INIT = 0,
	STATUS_IN_INIT,
	STATUS_INIT,
	STATUS_IN_CLOSE,
	STATUS_CLOSED
};

enum SweetSocket_peer_type
{
	PEER_NULL = 0,
	PEER_SERVER,
	PEER_CLIENT
};

enum SweetSocket_apply_on
{
	APPLY_ALL = -1,
};

enum SweetSocket_packet_type
{
	PACKET_NULL = 0,
	PACKET_DATA,
	PACKET_PING,
	PACKET_PONG,
	PACKET_CLOSE
};

enum SweetSocket_peer_pool_behaviour
{
	NO_POOL = 0,
	ONLY_RECIVE,
	ONLY_SEND,
	BOTH
};

struct SweetSocket_peer_data
{
	uint8_t type;
	char *addr;
	uint16_t port;
	uint64_t socket;
};

struct SweetSocket_peers
{
	struct SweetSocket_peer_connects *top;
	struct SweetSocket_peer_connects *base;
	uint64_t size;
};

struct SweetSocket_data_header
{
	enum SweetSocket_packet_type command;
	uint64_t size;
};

struct SweetSocket_data_pool
{
	char *data;
	uint64_t size;
	struct SweetSocket_data_pool *next;
};

struct SweetSocket_peer_clients
{
	struct SweetSocket_peer_data *client;
	struct SweetThread_identifyer reciveThread;
	struct SweetThread_identifyer sendThread;
	struct SweetSocket_data_pool *revice;
	struct SweetSocket_data_pool *send;
	uint64_t id;
	bool closing;
	struct SweetSocket_peer_clients *next;
};

struct SweetSocket_peer_connects
{
	uint64_t id;
	struct SweetSocket_peer_data socket;
	struct SweetThread_identifyer acceptThread;
	struct SweetSocket_peer_connects *next;
	struct SweetSocket_peer_connects *previous;
	bool enableRecivePool;
	bool enableSendPool;
};

struct SweetSocket_global_context
{
	struct SweetSocket_peers connections;
	enum SweetSocket_sweet_status_code status;
	enum SweetSocket_peer_type type;
	int64_t connectionsAlive;
	int64_t maxConnections;
	int64_t minClientID;
	struct SweetSocket_peer_clients *clients;
	bool useHeader;
};

struct SweetSocket_accept_data_context_thread
{
	struct SweetSocket_global_context *context;
	struct SweetSocket_peer_connects *connection;
	void (*functionSend)(void *, uint64_t, struct SweetSocket_global_context *, struct SweetSocket_peer_clients *, void *);
	void *intoExternaParmRecv;
	void (*functionRecv)(void *, uint64_t, struct SweetSocket_global_context *, struct SweetSocket_peer_clients *, void *);
	void *intoExternaParmSend;
};

struct SweetSocket_data_context_thread
{
	struct SweetSocket_global_context *context;
	struct SweetSocket_peer_clients *connection;
	void (*function)(void *, uint64_t, struct SweetSocket_global_context *, struct SweetSocket_peer_clients *, void *);
	void *intoExternaParm;
};

EXPORT struct SweetSocket_global_context *SweetSocket_initGlobalContext(enum SweetSocket_peer_type type);

EXPORT bool SweetSocket_closeGlobalContext(struct SweetSocket_global_context **context);

EXPORT int64_t SweetSocket_pushNewConnection(struct SweetSocket_peers *conn, struct SweetSocket_peer_connects *newSocket);

EXPORT bool SweetSocket_removeConnectionById(struct SweetSocket_peers *conn, uint64_t id);

EXPORT struct SweetSocket_peer_connects *SweetSocket_createPeer(struct SweetSocket_global_context *context, uint8_t type, const char *addr, uint16_t port);

EXPORT bool SweetSocket_peerOpenSocket(char *addr, int16_t port, struct addrinfo *hints, struct addrinfo **result, SOCKET *socketIdentifyer);

EXPORT bool SweetSocket_peerCloseSocket(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID);

EXPORT bool SweetSocket_peerClientClose(struct SweetSocket_global_context *context, enum SweetSocket_apply_on clientID);

EXPORT bool SweetSocket_sendData(const char *data, uint64_t size, struct SweetSocket_global_context *context, enum SweetSocket_apply_on clientID);

EXPORT bool SweetSocket_reciveData(struct SweetSocket_global_context *context, enum SweetSocket_apply_on clientID, struct SweetSocket_data_pool *target);

EXPORT bool SweetSocket_clientStartConnection(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID);

EXPORT bool SweetSocket_clientEnablePools(struct SweetSocket_global_context *context, enum SweetSocket_apply_on clientID, bool enableRecivePool, bool enableSendPool);

EXPORT bool SweetSocket_serverStartAccepting(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID, void *functionSend, void *functionRecv, void *parmsRecv, void *parmsSend, enum SweetSocket_peer_pool_behaviour pool);

EXPORT bool SweetSocket_serverStartListening(struct SweetSocket_global_context *context, enum SweetSocket_apply_on serverID);

EXPORT void SweetSocket_resolvePeer(struct SweetSocket_peer_clients *client);

//	Above is interal send and recive commands, this is not exported
bool SweetSocket_internalSend(char **data, uint64_t size, SOCKET id);

int64_t SweetSocket_internalRecive(char **data, uint64_t size, SOCKET id);
