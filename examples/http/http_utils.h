#include <stdbool.h>
#include <stdint.h>

bool HTTP_isDirectory(const wchar_t* path);

bool HTTP_isFile(const wchar_t* path);

bool HTTP_isKeepAlive(const char* request);

wchar_t* HTTP_findDefaultFile(struct HTTP_linked_list defaults, const wchar_t* path);

void HTTP_trimSpaces(wchar_t* str);

void HTTP_getTimeString(char* buffer, size_t bufferSize, int mode);

wchar_t* HTTP_getRequestPath(const char* request, wchar_t* root, wchar_t** virtual);

void HTTP_getRangeValues(const char* request, int64_t* start, int64_t* end);

void HTTP_logClientRequest(struct HTTP_server_config *server, const char *verb, const wchar_t *path, const char *userAgent, struct socketClients *client);

void HTTP_sendHeaderResponse(const char* mineType, uint16_t responseCode, uint64_t size, const char* opcionais, struct socketGlobalContext* context, uint64_t id);

void HTTP_sendErrorResponse(uint16_t code, const wchar_t* msg, struct socketGlobalContext* ctx, uint64_t id);

char* HTTP_getMimeType(const wchar_t* path, struct HTTP_linked_list* mineList);

char* HTTP_getVerb(const char* request);

char* HTTP_getUserAgent(const char* resquest, size_t size);

char* HTTP_getHost(const char* resquest);

size_t HTTP_createHtmlDirectoryList(const wchar_t* path, const wchar_t* virtualPath, wchar_t** html);