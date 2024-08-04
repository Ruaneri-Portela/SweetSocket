#include "http.h"
int closing = 0;

void HTTP_handleSigint(int sig)
{
	closing = 1;
}

void HTTP_callbackProcessRequest(char* data, uint64_t size, struct socketGlobalContext* ctx, struct socketClients* thisClient, void* parms)
{
	struct HTTPServerEnv* server = (struct HTTPServerEnv*)parms;
	int64_t requestedStartFile, requestedEndFile;
	HANDLE hFile = INVALID_HANDLE_VALUE;

	char path[PATHSIZE];
	const char* command = "200 OK";
	char* verb = HTTP_getVerb(data, sizeof(verb));
	bool routineAline = true;

	// Extrair informações da requisição PATH e RANGE
	HTTP_extractRange(data, &requestedStartFile, &requestedEndFile);
	HTTP_extractPath(data, path, sizeof(path));

	// Log
	char* userAgent = HTTP_getUserAgent(data, size);
	if (server->logFile != NULL)
	{
		HTTP_logRequest(server, verb, path, userAgent == NULL ? "" : userAgent,thisClient);
	}

	while (routineAline) {
		// Verificar se é um diretório
		while (HTTP_isDirectory(path))
		{
			// Tentar arquivo padrão
			char* defaultPage = (char*)malloc(strlen(path) + strlen(server->defaultPage) + 2);
			strcpy(defaultPage, path);
			if (defaultPage[strlen(defaultPage) - 1] != '/')
			{
				strcat(defaultPage, "/");
			}
			strcat(defaultPage, server->defaultPage);
			if (HTTP_isFile(defaultPage))
			{
				strcpy(path, defaultPage);
				free(defaultPage);
				break;
			}
			// Listagem de diretório
			routineAline = false;
			if (server->allowDirectoryListing == false)
			{
				HTTP_sendError(403, "<h1>403 Forbidden</h1><p>Directory listing not allowed.</p>", ctx, thisClient->id);
				free(defaultPage);
				break;
			}
			char* html = NULL;
			size_t htmlSize = HTTP_htmlListDir(path, &html);
			if (htmlSize == 0)
			{
				HTTP_sendError(404, "<h1>404 Not Found</h1><p>Directory not found.</p>", ctx, thisClient->id);
				free(defaultPage);
				break;
			}
			HTTP_sendHeader("text/html", command, htmlSize, NULL, ctx, thisClient->id);
			sendData(html, htmlSize, ctx, thisClient->id);
			free(html);
			free(defaultPage);
			break;
		}
		// Sair	
		if (!routineAline)
			break;

		// Quando for arquivo prosseguir
		hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hFile == INVALID_HANDLE_VALUE)
		{
			HTTP_sendError(404, "<h1>404 Not Found</h1><p>File not found.</p>", ctx, thisClient->id);
			break;
		}

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(hFile, &fileSize))
		{
			CloseHandle(hFile);
			HTTP_sendError(500, "<h1>500 Internal Server Error</h1><p>Cannot get file size.</p>", ctx, thisClient->id);
			break;
		}

		// Envio de arquivo
		char* type = HTTP_getMineType(path, server);
		char* options = NULL;

		// Verificar se é um range (parcial)
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
				HTTP_sendError(500, "<h1>500 Internal Server Error</h1><p>Cannot seek file.</p>", ctx, thisClient->id);
				break;
			}
			options = range;
		}

		// Enviar cabeçalho
		HTTP_sendHeader(type, command, fileSize.QuadPart, options, ctx, thisClient->id);
		free(type);
		// Enviar arquivo
		size_t sent = (fileSize.QuadPart > MAXFILESIZE) ? MAXFILESIZE : (size_t)fileSize.QuadPart;
		char* fileData = (char*)malloc(sent);
		if (fileData == NULL)
		{
			CloseHandle(hFile);
			HTTP_sendError(500, "<h1>500 Internal Server Error</h1><p>Cannot allocate memory.</p>", ctx, thisClient->id);
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
					continue;
				}
			}
			break;
		}
		CloseHandle(hFile);
		free(fileData);
		routineAline = false;
		break;
	}
	free(userAgent);
	free(verb);
	free(data);
}

int main(int argc, char* argv[])
{
	struct HTTPServerEnv server = HTTP_loadConf();
	struct socketGlobalContext* context = initSocketGlobalContext(SOCKET_SERVER);
	context->useHeader = false;
	for (char* host = server.hosts; host != NULL;)
	{
		char* nxtHost = strstr(host, ",");
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