#include "http_config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SweetSocket.h>
#include <time.h>
#include <wchar.h>

bool HTTP_isDirectory(const wchar_t* path)
{
	DWORD dwAttrib = GetFileAttributesW(path);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool HTTP_isFile(const wchar_t* path)
{
	DWORD dwAttrib = GetFileAttributesW(path);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool HTTP_isKeepAlive(const char* request)
{
	const char* keepAlive = strstr(request, "Connection: keep-alive");
	return keepAlive != NULL;
}

struct findFileDefaut
{
	const wchar_t* path;
	wchar_t* file;
};

static enum HTTP_linked_list_actions HTTP_isActualFile(struct HTTP_object* actual, void* parms, uint64_t count)
{
	(void)count;
	struct findFileDefaut* recive = (struct findFileDefaut*)parms;

	size_t lenReal = wcslen(recive->path);
	size_t lenFile = wcslen((wchar_t*)actual->object);

	wchar_t* file = malloc((lenFile + lenReal + 1) * sizeof(wchar_t));
	if (file == NULL)
	{
		perror("Memory allocation failed");
		return ARRAY_CONTINUE;
	}

	wmemcpy(file, recive->path, lenReal);
	wmemcpy(file + lenReal, (wchar_t*)actual->object, lenFile);

	file[lenReal + lenFile] = L'\0';

	if (HTTP_isFile(file))
	{
		recive->file = file;
		return ARRAY_STOP;
	}

	free(file);
	return ARRAY_CONTINUE;
}

wchar_t* HTTP_findDefaultFile(struct HTTP_linked_list defaults, const wchar_t* path)
{
	struct findFileDefaut parms = { (wchar_t*)path, NULL };
	HTTP_arrayForEach(&defaults, HTTP_isActualFile, &parms);
	return parms.file;
}

void HTTP_trimSpaces(wchar_t* str)
{
	wchar_t* end;

	while (iswspace(*str))
		str++;

	if (*str == L'\0')
		return;

	end = str + wcslen(str) - 1;

	while (end > str && iswspace(*end))
		end--;
	*(end + 1) = L'\0';
}

void HTTP_getTimeString(char* buffer, size_t bufferSize, int mode)
{
	time_t rawtime;
	struct tm* timeinfo;

	time(&rawtime);

	timeinfo = localtime(&rawtime);

	switch (mode)
	{
	case 0:
		strftime(buffer, bufferSize, "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
		break;
	case 1:
		strftime(buffer, bufferSize, "%d:%m:%Y %H:%M:%S", timeinfo);
	}
}


static wchar_t* HTTP_decodeURI(const char* uri) {
	size_t uriLen = strlen(uri);
	wchar_t* wdecoded = malloc((uriLen + 1) * sizeof(wchar_t));
	if (!wdecoded) {
		return NULL;
	}

	size_t decoded = 0;

	for (const char* p = uri; *p != '\0'; p++) {
		wchar_t utf16 = 0;

		if (*p == '%' && *(p + 1) && *(p + 2)) {
			char hex[3] = { *(p + 1), *(p + 2), '\0' };
			unsigned char firstByte = (unsigned char)strtol(hex, NULL, 16);
			p += 2;
			if (*(p + 1) == '%' && *(p + 2) && *(p + 3)) {
				hex[0] = *(p + 2);
				hex[1] = *(p + 3);
				unsigned char secondByte = (unsigned char)strtol(hex, NULL, 16);
				utf16 = ((firstByte & 0x1F) << 6) | (secondByte & 0x3F);
				p += 3;
			}
			else {
				utf16 = firstByte;
			}
		}
		else {
			utf16 = (wchar_t)(unsigned char)*p;
		}

		wdecoded[decoded++] = utf16;
	}

	wdecoded[decoded] = L'\0';
	wdecoded = realloc(wdecoded, (decoded + 1) * sizeof(wchar_t));
	if (wdecoded == NULL)
	{
		perror("Memory allocation failed");
		return NULL;
	}
	return wdecoded;
}

wchar_t* HTTP_getRequestPath(const char* request, wchar_t* root, wchar_t** virtual, wchar_t** pathContent)
{
	const char* start = strchr(request, ' ');
	if (start == NULL)
	{
		return NULL;
	}
	start++;
	const char* end = strchr(start, ' ');
	if (end == NULL)
	{
		return NULL;
	}
	size_t length = end - start;
	char* path = (char*)malloc(length + 1);
	if (path == NULL)
	{
		perror("Memory allocation failed");
		return NULL;
	}
	memcpy(path, start, length);
	path[length] = '\0';
	wchar_t* virtualPath = HTTP_decodeURI(path);
	free(path);
	size_t virtualPathLen = wcslen(virtualPath);
	size_t rootLen = wcslen(root);
	wchar_t* finalPath = malloc((virtualPathLen + rootLen + 1) * sizeof(wchar_t));
	if (finalPath == NULL)
	{
		perror("Memory allocation failed");
		return NULL;
	}
	wmemcpy(finalPath, root, rootLen);
	wmemcpy(finalPath + rootLen, virtualPath, virtualPathLen + 1);
	*virtual = finalPath + rootLen;
	free(virtualPath);

	wchar_t * pathEnd = wcschr(*virtual, L'?');
	if (pathEnd != NULL)
	{
		*pathEnd = L'\0';
		*pathContent = pathEnd + 1;
	}
	else
	{
		*pathContent = NULL;
	}
	return finalPath;
}

void HTTP_getRangeValues(const char* request, int64_t* start, int64_t* end)
{
	*start = -1;
	*end = -1;
	const char* range = strstr(request, "Range: bytes=");
	if (range == NULL)
		return;
	const char* rangeEnd = strchr(range, '\r');
	if (rangeEnd == NULL)
		return;
	const char* firstValue = range + 13;
	const char* dash = strchr(firstValue, '-');
	if (dash == NULL)
		return;
	const char* secondValue = dash + 1;
	if (secondValue < rangeEnd)
		*end = strtoll(secondValue, NULL, 10);
	*start = strtoll(firstValue, NULL, 10);
}

void HTTP_logClientRequest(struct HTTP_server_config* server, const char* verb, const wchar_t* path, const char* userAgent, struct SweetSocket_peer_clients* client)
{
	if (server->logFile == NULL)
		return;
	char* charPath = malloc((wcslen(path) + 1) * sizeof(char));
	wcstombs(charPath, path, wcslen(path) + 1);
	char* date = malloc(20);
	HTTP_getTimeString(date, 20, 1);
	if (client->client->addr == NULL)
		SweetSocket_resolvePeer(client);
	fprintf(server->logFile, "(%s) %s -> [%d] %s %s <- %s\n", date, client->client->addr, client->client->port, verb, charPath, userAgent);
	fflush(server->logFile);
	free(charPath);
	free(date);
}

static const char* HTTP_getStatusString(uint16_t responseCode)
{
	switch (responseCode)
	{
	case 100:
		return "100 Continue";
	case 101:
		return "101 Switching Protocols";
	case 102:
		return "102 Processing";
	case 200:
		return "200 OK";
	case 201:
		return "201 Created";
	case 202:
		return "202 Accepted";
	case 203:
		return "203 Non-Authoritative Information";
	case 204:
		return "204 No Content";
	case 205:
		return "205 Reset Content";
	case 206:
		return "206 Partial Content";
	case 207:
		return "207 Multi-Status";
	case 208:
		return "208 Already Reported";
	case 226:
		return "226 IM Used";
	case 300:
		return "300 Multiple Choices";
	case 301:
		return "301 Moved Permanently";
	case 302:
		return "302 Found";
	case 303:
		return "303 See Other";
	case 304:
		return "304 Not Modified";
	case 305:
		return "305 Use Proxy";
	case 307:
		return "307 Temporary Redirect";
	case 308:
		return "308 Permanent Redirect";
	case 400:
		return "400 Bad Request";
	case 401:
		return "401 Unauthorized";
	case 402:
		return "402 Payment Required";
	case 403:
		return "403 Forbidden";
	case 404:
		return "404 Not Found";
	case 405:
		return "405 Method Not Allowed";
	case 406:
		return "406 Not Acceptable";
	case 407:
		return "407 Proxy Authentication Required";
	case 408:
		return "408 Request Timeout";
	case 409:
		return "409 Conflict";
	case 410:
		return "410 Gone";
	case 411:
		return "411 Length Required";
	case 412:
		return "412 Precondition Failed";
	case 413:
		return "413 Payload Too Large";
	case 414:
		return "414 URI Too Long";
	case 415:
		return "415 Unsupported Media Type";
	case 416:
		return "416 Range Not Satisfiable";
	case 417:
		return "417 Expectation Failed";
	case 418:
		return "418 I'm a teapot";
	case 421:
		return "421 Misdirected Request";
	case 422:
		return "422 Unprocessable Entity";
	case 423:
		return "423 Locked";
	case 424:
		return "424 Failed Dependency";
	case 425:
		return "425 Too Early";
	case 426:
		return "426 Upgrade Required";
	case 428:
		return "428 Precondition Required";
	case 429:
		return "429 Too Many Requests";
	case 431:
		return "431 Request Header Fields Too Large";
	case 451:
		return "451 Unavailable For Legal Reasons";
	case 500:
		return "500 Internal Server Error";
	case 501:
		return "501 Not Implemented";
	case 502:
		return "502 Bad Gateway";
	case 503:
		return "503 Service Unavailable";
	case 504:
		return "504 Gateway Timeout";
	case 505:
		return "505 HTTP Version Not Supported";
	case 506:
		return "506 Variant Also Negotiates";
	case 507:
		return "507 Insufficient Storage";
	case 508:
		return "508 Loop Detected";
	case 510:
		return "510 Not Extended";
	case 511:
		return "511 Network Authentication Required";
	default:
		return "500 Internal Server Error";
	}
}

void HTTP_sendHeaderResponse(const char* mineType, uint16_t responseCode, uint64_t size, const char* opcionais, struct SweetSocket_global_context* context, uint64_t id)
{
	const char* status = HTTP_getStatusString(responseCode);
	const char* opts = opcionais == NULL ? "" : opcionais;
	const char* headerHttp = "HTTP/1.1 %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %lld\r\n"
		"Server: HttpSweetSocket\r\n"
		"Date: %s\r\n"
		"%s\r\n";
	char* date = (char*)malloc(30);
	if (date == NULL)
	{
		perror("Memory allocation failed");
		return;
	}
	HTTP_getTimeString(date, 30, 0);
	uint64_t sizeHeader = strlen(headerHttp) + strlen(mineType) + strlen(status) + strlen(opts) + 30 + 21;
	char* headerToSend = (char*)malloc(sizeHeader);
	if (headerToSend == NULL)
	{
		perror("Memory allocation failed");
		return;
	}
	snprintf(headerToSend, sizeHeader, headerHttp, status, mineType, size, date, opts);
	SweetSocket_sendData(headerToSend, strlen(headerToSend), context, id);
	free(headerToSend);
	free(date);
}

void HTTP_sendErrorResponse(uint16_t code, const wchar_t* msg, struct SweetSocket_global_context* ctx, uint64_t id)
{
	size_t msgSize = wcslen(msg) * sizeof(wchar_t);
	HTTP_sendHeaderResponse("text/html; charset=UTF-16", code, msgSize, NULL, ctx, id);
	SweetSocket_sendData((const char*)msg, msgSize, ctx, id);
}

struct HTTP_mime_parms
{
	const wchar_t* extension;
	const wchar_t* mineType;
};

static wchar_t* HTTP_findLasDot(const wchar_t* path)
{
	wchar_t* dot = NULL;
	for (const wchar_t* actual = path; *actual != L'\0'; actual++)
	{
		if (*actual == L'.')
		{
			dot = (wchar_t*)actual;
		}
	}
	return dot;
}

static enum HTTP_linked_list_actions HTTP_locateMimeType(struct HTTP_object* actual, void* parms, uint64_t count)
{
	(void)count;
	struct HTTP_server_mine_type* mineType = (struct HTTP_server_mine_type*)actual->object;
	struct HTTP_mime_parms* localParms = (struct HTTP_mime_parms*)parms;
	if (wcscmp(localParms->extension, mineType->extension) == 0)
	{
		localParms->mineType = mineType->mineType;
		return ARRAY_STOP;
	}
	return ARRAY_CONTINUE;
}

char* HTTP_getMimeType(const wchar_t* path, struct HTTP_linked_list* mineList)
{
	struct HTTP_mime_parms parms = { NULL, NULL };
	parms.extension = HTTP_findLasDot(path);
	if (parms.extension == NULL)
	{
		return NULL;
	}
	parms.extension++;
	HTTP_arrayForEach(mineList, HTTP_locateMimeType, &parms);
	if (parms.mineType == NULL)
		return NULL;
	char* mineType = malloc((wcslen(parms.mineType) + 1) * sizeof(char));
	wcstombs(mineType, parms.mineType, wcslen(parms.mineType) + 1);
	return mineType;
}

char* HTTP_getVerb(const char* request)
{
	const char* end = strchr(request, ' ');
	if (end == NULL)
	{
		return NULL;
	}
	size_t length = end - request;
	char* verb = (char*)malloc(length + 1);
	if (verb == NULL)
	{
		perror("Memory allocation failed");
		return NULL;
	}
	strncpy(verb, request, length);
	verb[length] = '\0';
	return verb;
}

char* HTTP_getUserAgent(const char* resquest, size_t size)
{
	const char* start = strstr(resquest, "User-Agent: ");
	if (start == NULL)
	{
		return NULL;
	}
	start += 12;
	const char* end = strstr(start, "\r\n");
	if (end == NULL)
	{
		return NULL;
	}
	size_t length = end - start;
	if (length >= size)
	{
		length = size - 1;
	}
	char* userAgent = (char*)malloc(length + 1);
	if (userAgent == NULL)
	{
		perror("Memory allocation failed");
		return NULL;
	}
	strncpy(userAgent, start, length);
	userAgent[length] = '\0';
	return userAgent;
}

char* HTTP_getHost(const char* resquest)
{
	const char* hostTerminator = ":";
	const char* start = strstr(resquest, ": ");
	size_t length = 0;
	if (start == NULL)
	{
		return NULL;
	}
	if (start[2] == '[')
	{
		hostTerminator = "]";
		length++;
	}
	const char* end = strstr(start + 2, hostTerminator);
	if (end == NULL)
	{
		return NULL;
	}
	length += end - start - 2;
	char* host = (char*)malloc(length + 1);
	if (host == NULL)
	{
		perror("Memory allocation failed");
		return NULL;
	}
	memcpy(host, start + 2, length);
	host[length] = '\0';
	return host;
}

size_t HTTP_createHtmlDirectoryList(const wchar_t* path, const wchar_t* virtualPath, wchar_t** html)
{
	const wchar_t* htmlDocument = L"<!DOCTYPE html><html><head><meta charset=\"UTF-16\"><title>%s</title></head><body><h1>Files</h1><ul>%s</ul></body></html>";
	const wchar_t* listItem = L"<li><a href=\"%s%s\">%s</a></li>";
	const wchar_t* emptyList = L"<li>No files</li>";
	wchar_t* data = NULL;
	size_t virtualPathLen = wcslen(virtualPath);
	size_t hmtlDocumentSize = wcslen(htmlDocument);
	size_t listItemSize = wcslen(listItem);
	size_t pathLen = wcslen(path);
	size_t dataSize = 0;
	bool freeName = false;

	// List files
	WIN32_FIND_DATAW findFileData;
	wchar_t* pathCopy = malloc((pathLen + 2) * sizeof(wchar_t));
	wmemcpy(pathCopy, path, pathLen);
	pathCopy[pathLen] = L'*';
	pathCopy[pathLen + 1] = L'\0';

	HANDLE hFind = FindFirstFileW(pathCopy, &findFileData);
	free(pathCopy);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	do
	{
		wchar_t* name = findFileData.cFileName;
		size_t nameSize = wcslen(name);
		if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0)
			continue;

		wchar_t* pathName = malloc((pathLen + nameSize + 2) * sizeof(wchar_t));
		wmemcpy(pathName, path, pathLen);
		wmemcpy(pathName + pathLen, name, nameSize + 1);

		if (HTTP_isDirectory(pathName))
		{
			nameSize++;
			name = malloc((nameSize + 1) * sizeof(wchar_t));
			wmemcpy(name, findFileData.cFileName, nameSize - 1);
			name[nameSize - 1] = L'/';
			name[nameSize] = L'\0';
			freeName = true;
		}
		free(pathName);

		size_t localSize = listItemSize + virtualPathLen + (nameSize * 2);
		wchar_t* localData = malloc((localSize) * sizeof(wchar_t));

		swprintf(localData, localSize, listItem, virtualPath, name, name);

		if (data == NULL)
		{
			data = localData;
			dataSize = localSize;
		}
		else
		{
			data = realloc(data, (dataSize + localSize) * sizeof(wchar_t));
			if (data == NULL)
			{
				perror("Memory allocation failed");
				free(localData);
				return 0;
			}
			wcscat(data, localData);
			dataSize += localSize;
			free(localData);
		}

		if (freeName)
		{
			free(name);
			freeName = false;
		}
	} while (FindNextFileW(hFind, &findFileData) != 0);
	FindClose(hFind);
	if (dataSize == 0)
	{
		dataSize += hmtlDocumentSize + wcslen(emptyList) + virtualPathLen;
		*html = malloc(dataSize * sizeof(wchar_t));
		if (*html == NULL)
		{
			perror("Memory allocation failed");
			return 0;
		}
		swprintf(*html, dataSize, htmlDocument, virtualPath, emptyList);
	}
	else
	{
		dataSize += hmtlDocumentSize + virtualPathLen;
		*html = malloc(dataSize * sizeof(wchar_t));
		if (*html == NULL)
		{
			perror("Memory allocation failed");
			free(data);
			return 0;
		}
		swprintf(*html, dataSize, htmlDocument, virtualPath, data);
		free(data);
	}
	return wcslen(*html) * sizeof(wchar_t);
}

void HTTP_splitRequest(char* request, uint64_t resquestSize, char** header, char** data, uint64_t* dataSize)
{
	char* end = strstr(request, "\r\n\r\n");
	if (end == NULL)
	{
		*header = NULL;
		*data = NULL;
		return;
	}
	size_t headerSize = end - request + 4;
	request[headerSize - 1] = '\0';
	*header = request;
	if (resquestSize == headerSize)
	{
		*data = NULL;
		return;
	}
	*dataSize = resquestSize - headerSize;
	*data = request + headerSize;
	return;
}