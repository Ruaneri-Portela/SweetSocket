#include "http_config.h"
#include "http_plugin.h"
#include "http_process.h"
#include "http_utils.h"
#include <SweetSocket.h>

/**
 * Envia os dados de um arquivo para o cliente.
 *
 * @param parms Parâmetros da solicitação HTTP.
 * @param context Contexto global do socket.
 * @param thisClient Cliente que está fazendo a solicitação.
 */
static void HTTP_transferData(struct HTTPsendParms *parms, struct SweetSocket_global_context *context, struct SweetSocket_peer_clients *thisClient)
{
	char *options = NULL;
	char *type = NULL;
	uint16_t command = 200;

	// Abrir o arquivo
	HANDLE hFile = CreateFileW(parms->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		HTTP_sendErrorResponse(404, L"<h1>404 Not Found</h1><p>File not found.</p>", context, thisClient->id);
		return;
	}

	// Obter o tamanho do arquivo
	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(hFile, &fileSize))
	{
		CloseHandle(hFile);
		HTTP_sendErrorResponse(500, L"<h1>500 Internal Server Error</h1><p>Cannot get file size.</p>", context, thisClient->id);
		return;
	}

	// Verificar se é um intervalo de bytes (parcial)
	if (parms->requestedStartFile != -1 || parms->requestedEndFile != -1)
	{
		const char *format = "Accept-Ranges: bytes\r\nContent-Range: bytes %lld-%lld/%lld\r\n";
		options = (char *)malloc(100);
		if (!options)
		{
			CloseHandle(hFile);
			HTTP_sendErrorResponse(500, L"<h1>500 Internal Server Error</h1><p>Cannot allocate memory for headers.</p>", context, thisClient->id);
			return;
		}

		int64_t physicalFileSize = fileSize.QuadPart;
		parms->requestedEndFile = (parms->requestedEndFile == -1) ? fileSize.QuadPart - 1 : parms->requestedEndFile;
		snprintf(options, 100, format, parms->requestedStartFile, parms->requestedEndFile, fileSize.QuadPart);
		fileSize.QuadPart = parms->requestedEndFile - parms->requestedStartFile + 1;
		if (fileSize.QuadPart != physicalFileSize)
			command = 206;

		LARGE_INTEGER liOffset;
		liOffset.QuadPart = parms->requestedStartFile;
		if (SetFilePointerEx(hFile, liOffset, NULL, FILE_BEGIN) == 0)
		{
			CloseHandle(hFile);
			HTTP_sendErrorResponse(500, L"<h1>500 Internal Server Error</h1><p>Cannot seek file.</p>", context, thisClient->id);
			return;
		}
	}

	// Enviar cabeçalho
	type = HTTP_getMimeType(parms->path, &parms->envolvirment->mimeTypes);
	HTTP_sendHeaderResponse(type == NULL ? "application/octet-stream" : type, command, fileSize.QuadPart, options, context, thisClient->id);
	if (type != NULL)
		free(type);
	if (options != NULL)
		free(options);

	// Enviar o arquivo
	int64_t sent = (fileSize.QuadPart > parms->envolvirment->server.partialMaxFileBlock) ? parms->envolvirment->server.partialMaxFileBlock : fileSize.QuadPart;
	char *fileData = (char *)malloc(sent);
	if (fileData == NULL)
	{
		CloseHandle(hFile);
		HTTP_sendErrorResponse(500, L"<h1>500 Internal Server Error</h1><p>Cannot allocate memory for file data.</p>", context, thisClient->id);
		return;
	}

	for (int64_t totalSent = 0; totalSent < fileSize.QuadPart;)
	{
		int64_t toRead = (fileSize.QuadPart - totalSent > parms->envolvirment->server.partialMaxFileBlock) ? parms->envolvirment->server.partialMaxFileBlock : (size_t)(fileSize.QuadPart - totalSent);
		DWORD readSize;
		if (ReadFile(hFile, fileData, toRead, &readSize, NULL))
		{
			if (readSize > 0)
			{
				if (!SweetSocket_sendData(fileData, readSize, context, thisClient->id))
					break;
				totalSent += readSize;
				continue;
			}
		}
		break; // Se não conseguiu ler, sair do loop
	}
	CloseHandle(hFile);
	free(fileData);
}

/**
 * Envia a listagem de diretórios para o cliente.
 *
 * @param parms Parâmetros da solicitação HTTP.
 * @param context Contexto global do socket.
 * @param thisClient Cliente que está fazendo a solicitação.
 */
static void HTTP_sendDirectoryListing(struct HTTPsendParms *parms, struct SweetSocket_global_context *context, struct SweetSocket_peer_clients *thisClient)
{
	if (!parms->envolvirment->server.allowDirectoryListing)
	{
		HTTP_sendErrorResponse(403, L"<h1>403 Forbidden</h1><p>Directory listing not allowed.</p>", context, thisClient->id);
		return;
	}

	wchar_t *html = NULL;
	size_t htmlSize = HTTP_createHtmlDirectoryList(parms->path, parms->virtualPath, &html);
	if (htmlSize == 0)
	{
		HTTP_sendErrorResponse(404, L"<h1>404 Not Found</h1><p>Directory not found.</p>", context, thisClient->id);
		return;
	}

	HTTP_sendHeaderResponse("text/html; charset=UTF-16", 200, htmlSize, NULL, context, thisClient->id);
	SweetSocket_sendData((const char *)html, htmlSize, context, thisClient->id);
	free(html);
}

/**
 * Processa a solicitação do cliente HTTP.
 *
 * @param data Dados da solicitação.
 * @param size Tamanho dos dados da solicitação.
 * @param ctx Contexto global do socket.
 * @param thisClient Cliente que está fazendo a solicitação.
 * @param parms Parâmetros da solicitação HTTP.
 */
void HTTP_processClientRequest(char *data, uint64_t size, struct SweetSocket_global_context *ctx, struct SweetSocket_peer_clients *thisClient, void *parms)
{
	struct HTTP_server_envolvirment *envolvirment = (struct HTTP_server_envolvirment *)parms;

	// Separar cabeçalho e corpo
	char *header = NULL, *body = NULL;
	uint64_t dataSize = 0;
	HTTP_splitRequest(data, size, &header, &body, &dataSize);

	// Trata o caminho da request
	wchar_t *virtualPath = NULL;
	wchar_t *realPath = HTTP_getRequestPath(data, envolvirment->server.root, &virtualPath);

	// Obter informações da solicitação
	char *userAgent = HTTP_getUserAgent(data, size);
	char *verb = HTTP_getVerb(data);
	char *host = HTTP_getHost(data);

	HTTP_logClientRequest(&envolvirment->server, verb, virtualPath, userAgent == NULL ? "" : userAgent, (void *)thisClient);

	// Processar plugins
	// Essa parte não está completamente implementanda ou definida
	for (struct HTTP_object *plugin = envolvirment->plugins.base; plugin != NULL; plugin = plugin->next)
	{
		struct HTTP_plugin_metadata *metadata = (struct HTTP_plugin_metadata *)plugin->object;
		if (!metadata->isKeepLoaded)
			metadata->entryPoint();

		char *pluginContent = NULL;
		uint64_t pluginResponseSize = 0;
		uint16_t pluginResponseCode = 0;
		char *pluginAdditionalHeader = NULL;
		char *typeResponse = NULL;
		if (metadata->responsePoint(header, body, dataSize, &pluginContent, &pluginResponseSize, &pluginResponseCode, &typeResponse, &pluginAdditionalHeader))
		{
			HTTP_sendHeaderResponse(typeResponse, pluginResponseCode, pluginResponseSize, pluginAdditionalHeader, ctx, thisClient->id);
			SweetSocket_sendData(pluginContent, pluginResponseSize, ctx, thisClient->id);
			free(pluginContent);
			free(pluginAdditionalHeader);
			if (!HTTP_isKeepAlive(data))
				SweetSocket_peerClientClose(ctx, thisClient->id);
			return;
		}
		if (!metadata->isKeepLoaded)
			metadata->shutdownPoint();
	}

	struct HTTPsendParms sendParms = {realPath, virtualPath, -1, -1, envolvirment};

	// Verificar se é um diretório
	if (HTTP_isDirectory(realPath))
	{
		size_t len = wcslen(realPath);
		size_t lenV = wcslen(virtualPath);
		if (realPath[len - 1] != L'/')
		{
			len += 1;
			realPath = realloc(realPath, (len + 1) * sizeof(wchar_t));
			if (realPath == NULL)
			{
				HTTP_sendErrorResponse(500, L"<h1>500 Internal Server Error</h1><p>Memory allocation failed.</p>", ctx, thisClient->id);
				goto HTTPexit;
			}
			realPath[len - 1] = L'/';
			realPath[len] = L'\0';
			lenV += 1;
		}

		// Tentar arquivo padrão
		wchar_t *defaultPage = HTTP_findDefaultFile(envolvirment->server.defaultPages, realPath);
		if (defaultPage != NULL)
		{
			free(realPath);
			realPath = defaultPage;
			sendParms.path = realPath;
			HTTP_transferData(&sendParms, ctx, thisClient);
			goto HTTPexit;
		}

		// Listar diretório
		if (envolvirment->server.allowDirectoryListing)
		{
			virtualPath = realPath + (len - lenV);
			sendParms.virtualPath = virtualPath;
			sendParms.path = realPath;
			HTTP_sendDirectoryListing(&sendParms, ctx, thisClient);
			goto HTTPexit;
		}
	}
	else if (HTTP_isFile(realPath))
	{
		// Verificar se é um arquivo
		HTTP_getRangeValues(data, &sendParms.requestedStartFile, &sendParms.requestedEndFile);
		HTTP_transferData(&sendParms, ctx, thisClient);
		goto HTTPexit;
	}

	// Arquivo não encontrado
	HTTP_sendErrorResponse(404, L"<h1>404 Not Found</h1><p>File not found.</p>", ctx, thisClient->id);

HTTPexit:
	free(userAgent);
	free(host);
	free(verb);
	free(realPath);
	if (!HTTP_isKeepAlive(data))
		SweetSocket_peerClientClose(ctx, thisClient->id);
}