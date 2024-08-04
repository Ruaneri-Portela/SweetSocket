#include "http.h"
int closing = 0;

void HTTP_handleSigint(int sig)
{
	closing = 1;
}

void HTTP_callbackProcessRequest(char *data, uint64_t size, struct socketGlobalContext *ctx, struct socketClients *thisClient, void *parms)
{
	struct HTTPServerEnv *server = (struct HTTPServerEnv *)parms;
	int64_t requestedStartFile, requestedEndFile;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	char path[PATHSIZE];
	char header[HEADER_SIZE];
	char *verb = HTTP_getVerb(data, sizeof(verb));

	// Extrair informações da requisição PATH e RANGE
	HTTP_extractRange(data, &requestedStartFile, &requestedEndFile);
	HTTP_extractPath(data, path, sizeof(path));

	// Log
	if (server->logFile != NULL)
	{
		HTTP_logRequest(server, verb, path, thisClient);
	}

	while (HTTP_isDirectory(path))
	{
		// Tentar arquivo padrão
		char *defaultPage = (char *)malloc(strlen(path) + strlen(server->defaultPage) + 1);
		strcpy(defaultPage, path);
		strcat(defaultPage, server->defaultPage);
		if (HTTP_isFile(defaultPage))
		{
			strcpy(path, defaultPage);
			free(defaultPage);
			break;
		}
		// Listagem de diretório
		if (server->allowDirectoryListing == false)
		{
			const char *error_response = "HTTP/1.1 403 Forbidden\r\n"
										 "Content-Type: text/html\r\n"
										 "Content-Length: 44\r\n"
										 "Server: HttpSweetSocket\r\n"
										 "\r\n"
										 "<h1>403 Forbidden</h1><p>Directory listing not allowed.</p>";
			sendData(error_response, strlen(error_response), ctx, thisClient->id);
			free(defaultPage);
			free(verb);
			return;
		}
		char *html = NULL;
		size_t htmlSize = HTTP_htmlListDir(path, &html);
		snprintf(header, sizeof(header),
				 "HTTP/1.1 200 OK\r\n"
				 "Content-Type: text/html\r\n"
				 "Content-Length: %lld\r\n"
				 "Server: HttpSweetSocket\r\n"
				 "\r\n",
				 htmlSize);
		sendData(header, strlen(header), ctx, thisClient->id);
		sendData(html, htmlSize, ctx, thisClient->id);
		free(html);
		free(defaultPage);
		free(verb);
		return;
	}
	hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		const char *error_response = "HTTP/1.1 404 Not Found\r\n"
									 "Content-Type: text/html\r\n"
									 "Content-Length: 44\r\n"
									 "Server: HttpSweetSocket\r\n"
									 "\r\n"
									 "<h1>404 Not Found</h1><p>File not found.</p>";
		sendData(error_response, strlen(error_response), ctx, thisClient->id);
		free(verb);
		return;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(hFile, &fileSize))
	{
		CloseHandle(hFile);
		const char *error_response = "HTTP/1.1 500 Internal Server Error\r\n"
									 "Content-Type: text/html\r\n"
									 "Content-Length: 50\r\n"
									 "Server: HttpSweetSocket\r\n"
									 "\r\n"
									 "<h1>500 Internal Server Error</h1><p>Cannot get file size.</p>";
		sendData(error_response, strlen(error_response), ctx, thisClient->id);
		free(verb);
		return;
	}

	const char *type = HTTP_mineType(path, server);
	const char *command = "200 OK";
	char *options = NULL;

	if (requestedStartFile != -1 || requestedEndFile != -1)
	{
		char range[256];
		int64_t physicalFileSize = fileSize.QuadPart;
		requestedEndFile = (requestedEndFile == -1) ? fileSize.QuadPart - 1 : requestedEndFile;
		snprintf(range, sizeof(range),
				 "Accept-Ranges: bytes\r\n"
				 "Content-Range: bytes %lld-%lld/%lld\r\n",
				 requestedStartFile,
				 requestedEndFile,
				 fileSize.QuadPart);
		fileSize.QuadPart = requestedEndFile - requestedStartFile + 1;
		if (fileSize.QuadPart != physicalFileSize)
		{
			command = "206 Partial Content";
		}
		LARGE_INTEGER liOffset;
		liOffset.QuadPart = requestedStartFile;
		if (SetFilePointerEx(hFile, liOffset, NULL, FILE_BEGIN) == 0)
		{
			CloseHandle(hFile);
			const char *error_response = "HTTP/1.1 500 Internal Server Error\r\n"
										 "Content-Type: text/html\r\n"
										 "Content-Length: 50\r\n"
										 "Server: HttpSweetSocket\r\n"
										 "\r\n"
										 "<h1>500 Internal Server Error</h1><p>Cannot seek file.</p>";
			sendData(error_response, strlen(error_response), ctx, thisClient->id);
			free(verb);
			return;
		}
		options = range;
	}

	snprintf(header, sizeof(header),
			 "HTTP/1.1 %s\r\n"
			 "Content-Type: %s\r\n"
			 "Content-Length: %lld\r\n"
			 "Server: HttpSweetSocket\r\n"
			 "%s"
			 "\r\n",
			 command,
			 type,
			 fileSize.QuadPart,
			 options == NULL ? "" : options);
	sendData(header, strlen(header), ctx, thisClient->id);

	size_t sent = (fileSize.QuadPart > MAXFILESIZE) ? MAXFILESIZE : (size_t)fileSize.QuadPart;
	char *fileData = (char *)malloc(sent);
	if (fileData == NULL)
	{
		CloseHandle(hFile);
		const char *error_response = "HTTP/1.1 500 Internal Server Error\r\n"
									 "Content-Type: text/html\r\n"
									 "Content-Length: 50\r\n"
									 "Server: HttpSweetSocket\r\n"
									 "\r\n"
									 "<h1>500 Internal Server Error</h1><p>Cannot allocate memory.</p>";
		sendData(error_response, strlen(error_response), ctx, thisClient->id);
		free(verb);
		return;
	}

	size_t totalSent = 0;
	while (totalSent < fileSize.QuadPart)
	{
		size_t toRead = (fileSize.QuadPart - totalSent > MAXFILESIZE) ? MAXFILESIZE : (size_t)(fileSize.QuadPart - totalSent);
		DWORD readSize;
		if (ReadFile(hFile, fileData, toRead, &readSize, NULL))
		{
			if (readSize > 0)
			{
				if (!sendData(fileData, readSize, ctx, thisClient->id))
					break;
				totalSent += readSize;
			}
			else
			{
				// Erro na leitura do arquivo
				break;
			}
		}
		else
		{
			// Erro na leitura do arquivo
			break;
		}
	}

	CloseHandle(hFile);
	free(fileData);
	free(verb);
	free(data);
}

int main(int argc, char *argv[])
{
	struct HTTPServerEnv server = HTTP_loadConf();
	struct socketGlobalContext *context = initSocketGlobalContext(SOCKET_SERVER);
	context->useHeader = false;
	for (char *host = server.hosts; host != NULL;)
	{
		char *nxtHost = strstr(host, ",");
		if (nxtHost != NULL)
		{
			*nxtHost = '\0';
			nxtHost++;
		}
		if (strchr(host, ':') != NULL)
		{
			pushNewConnection(&context->connections, createSocket(context, AF_INET6, host, server.port));
		}
		else if (strchr(host, '.') != NULL)
		{
			pushNewConnection(&context->connections, createSocket(context, AF_INET, host, server.port));
		}
		host = nxtHost;
	}
	startListening(context, APPLY_ALL);
	startAccepting(context, APPLY_ALL, NULL, &HTTP_callbackProcessRequest, &server, NULL, ONLY_RECIVE);
	signal(SIGINT, HTTP_handleSigint);
	printf("Pressione Ctrl-C para sair...\n");
	while (!closing)
	{
		sweetThread_Sleep(1000);
	}
	closeSocketGlobalContext(&context);
	return 0;
}
