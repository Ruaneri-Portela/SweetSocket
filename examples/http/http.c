#define _CRT_SECURE_NO_WARNINGS
#include <signal.h>
#include <stdio.h>
#include <SweetSocket.h>

#define PATHSIZE 512
#define MAXFILESIZE 524288
int closing = 0;

void extractPath(const char *request, char *path, size_t path_size)
{
	const char *start = strchr(request, ' ') + 1;
	if (start == NULL)
	{
		strncpy(path, "", path_size);
		return;
	}

	const char *end = strchr(start, ' ');
	if (end == NULL)
	{
		strncpy(path, "", path_size);
		return;
	}

	size_t length = end - start;
	if (length >= path_size)
	{
		length = path_size - 1;
	}
	strncpy(path + 1, start, length);
	path[length + 1] = '\0';
	path[0] = '.';
}

void processRevc(char *data, uint64_t size, struct socketGlobalContext *ctx, struct socketClients *thisClient, void *parms)
{
	char path[PATHSIZE];
	extractPath(data, path, sizeof(path));

	if (strcmp(path, "./") == 0 || strcmp(path, "/") == 0)
	{
		strcpy(path, "index.html");
	}

	FILE *file = fopen(path, "rb");
	if (file == NULL)
	{
		const char *error_response = "HTTP/1.1 404 Not Found\r\n"
									 "Content-Type: text/html\r\n"
									 "Content-Length: 44\r\n"
									 "\r\n"
									 "<h1>404 Not Found</h1><p>File not found.</p>";
		sendData(error_response, strlen(error_response), ctx, thisClient->id);
		return;
	}

	int64_t fileSize = 0;

#ifdef WINSWEETSOCKET
	HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	GetFileSizeEx(hFile, (PLARGE_INTEGER)(&fileSize));
	CloseHandle(hFile);
#endif

	char *type;
	if (strstr(path, ".html"))
	{
		type = "text/html";
	}
	else if (strstr(path, ".mp4"))
	{
		type = "video/mp4";
	}
	else if (strstr(path, ".mkv"))
	{
		type = "video/x-matroska";
	}
	else
	{
		type = "text/plain";
	}

	char header[256];
	snprintf(header, sizeof(header),
			 "HTTP/1.1 200 OK\r\n"
			 "Content-Type: %s\r\n"
			 "Content-Length: %lld\r\n"
			 "\r\n",
			 type,
			 (long long)fileSize);
	sendData(header, strlen(header), ctx, thisClient->id);

	// Envio do arquivo em partes
	size_t sent = (fileSize > MAXFILESIZE) ? MAXFILESIZE : fileSize;
	char *fileData = (char *)malloc(sent);
	if (fileData == NULL)
	{
		fclose(file);
		const char *error_response = "HTTP/1.1 500 Internal Server Error\r\n"
									 "Content-Type: text/html\r\n"
									 "Content-Length: 50\r\n"
									 "\r\n"
									 "<h1>500 Internal Server Error</h1><p>Cannot allocate memory.</p>";
		sendData(error_response, strlen(error_response), ctx, thisClient->id);
		return;
	}

	size_t totalSent = 0;
	while (totalSent < fileSize)
	{
		size_t toRead = (fileSize - totalSent > MAXFILESIZE) ? MAXFILESIZE : (fileSize - totalSent);
		size_t readSize = fread(fileData, 1, toRead, file);
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

	fclose(file);
	free(fileData);
	free(data);
}

void handleSigint(int sig)
{
	printf("Recebi o sinal SIGINT (%d). Encerrando...\n", sig);
	closing = 1;
}

int main(int argc, char *argv[])
{
	struct socketGlobalContext *context = initSocketGlobalContext(SOCKET_SERVER);
	context->useHeader = false;
	pushNewConnection(&context->connections, createSocket(context, AF_INET, NULL, 9950));
	pushNewConnection(&context->connections, createSocket(context, AF_INET6, NULL, 9950));
	startListening(context, APPLY_ALL);
	startAccepting(context, APPLY_ALL, NULL, &processRevc, NULL, NULL, ONLY_RECIVE);
	signal(SIGINT, handleSigint);
	printf("Pressione Ctrl-C para sair...\n");
	while (!closing)
	{
		sweetThread_Sleep(1000);
	}
	closeSocketGlobalContext(&context);
	return 0;
}