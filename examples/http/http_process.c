#include "http_config.h"
#include "http_plugin.h"
#include "http_utils.h"
#include <wchar.h>

static void HTTP_transferData(struct HTTP_request *request, struct SweetSocket_global_context *context, struct SweetSocket_peer_clients *thisClient)
{
	char *options = NULL;
	char *type = NULL;
	uint16_t command = 200;

	// Abrir o arquivo
	HANDLE hFile = CreateFileW(request->filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
	if (request->startRange != -1 || request->endRange != -1)
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
		request->endRange = (request->endRange == -1) ? fileSize.QuadPart - 1 : request->endRange;
		snprintf(options, 100, format, request->startRange, request->endRange, fileSize.QuadPart);
		fileSize.QuadPart = request->endRange - request->startRange + 1;
		if (fileSize.QuadPart != physicalFileSize)
			command = 206;

		LARGE_INTEGER liOffset;
		liOffset.QuadPart = request->startRange;
		if (SetFilePointerEx(hFile, liOffset, NULL, FILE_BEGIN) == 0)
		{
			CloseHandle(hFile);
			HTTP_sendErrorResponse(500, L"<h1>500 Internal Server Error</h1><p>Cannot seek file.</p>", context, thisClient->id);
			return;
		}
	}

	// Enviar cabeçalho
	type = HTTP_getMimeType(request->filePath, &request->envolvirment->mimeTypes);
	HTTP_sendHeaderResponse(type == NULL ? "application/octet-stream" : type, command, fileSize.QuadPart, options, context, thisClient->id);
	if (type != NULL)
		free(type);
	if (options != NULL)
		free(options);

	// Enviar o arquivo
	int64_t sent = (fileSize.QuadPart > request->envolvirment->server.partialMaxFileBlock) ? request->envolvirment->server.partialMaxFileBlock : fileSize.QuadPart;
	char *fileData = (char *)malloc(sent);
	if (fileData == NULL)
	{
		CloseHandle(hFile);
		HTTP_sendErrorResponse(500, L"<h1>500 Internal Server Error</h1><p>Cannot allocate memory for file data.</p>", context, thisClient->id);
		return;
	}

	for (int64_t totalSent = 0; totalSent < fileSize.QuadPart;)
	{
		int64_t toRead = (fileSize.QuadPart - totalSent > request->envolvirment->server.partialMaxFileBlock) ? request->envolvirment->server.partialMaxFileBlock : (size_t)(fileSize.QuadPart - totalSent);
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

static void HTTP_sendDirectoryListing(struct HTTP_request *request, struct SweetSocket_global_context *context, struct SweetSocket_peer_clients *thisClient)
{
	if (!request->envolvirment->server.allowDirectoryListing)
	{
		HTTP_sendErrorResponse(403, L"<h1>403 Forbidden</h1><p>Directory listing not allowed.</p>", context, thisClient->id);
		return;
	}

	wchar_t *html = NULL;
	size_t htmlSize = HTTP_createHtmlDirectoryList(request->filePath, request->virtualPath, &html);
	if (htmlSize == 0)
	{
		HTTP_sendErrorResponse(404, L"<h1>404 Not Found</h1><p>Directory not found.</p>", context, thisClient->id);
		return;
	}

	HTTP_sendHeaderResponse("text/html; charset=UTF-16", 200, htmlSize, NULL, context, thisClient->id);
	SweetSocket_sendData((const char *)html, htmlSize, context, thisClient->id);
	free(html);
}

static struct HTTP_request HTTP_requestConstruct(char *data, uint64_t size, struct HTTP_server_envolvirment *envolvirment)
{
	struct HTTP_request request = {0};
	HTTP_splitRequest(data, size, &request.header, &request.data, &request.dataSize);
	request.verb = HTTP_getVerb(data);
	request.host = HTTP_getHost(data);
	request.userAgent = HTTP_getUserAgent(data, size);
	HTTP_parsingUrl(data, envolvirment->server.root, &request.filePath, &request.virtualPath, &request.getContent);
	request.envolvirment = envolvirment;
	request.startRange = -1;
	request.endRange = -1;
	request.useRange = HTTP_getRangeValues(data, &request.startRange, &request.endRange);
	return request;
}

static void HTTP_requestDestroy(struct HTTP_request *request)
{
	free(request->filePath);
	free(request->verb);
	free(request->userAgent);
	free(request->host);
}

void HTTP_processClientRequest(char *data, uint64_t size, struct SweetSocket_global_context *ctx, struct SweetSocket_peer_clients *thisClient, void *parms)
{
	struct HTTP_server_envolvirment *envolvirment = (struct HTTP_server_envolvirment *)parms;

	struct HTTP_request request = HTTP_requestConstruct(data, size, envolvirment);

	HTTP_logClientRequest(&request, thisClient);

	// Processar plugins
	// Essa parte não está completamente implementanda ou definida
	for (struct HTTP_object *plugin = envolvirment->plugins.base; plugin != NULL; plugin = plugin->next)
	{
		struct HTTP_plugin_metadata *metadata = (struct HTTP_plugin_metadata *)plugin->object;
		if (!metadata->isKeepLoaded)
			metadata->entryPoint();

		char *pluginContent = NULL;
		char *pluginAdditionalHeader = NULL;
		char *typeResponse = NULL;
		uint64_t pluginResponseSize = 0;
		uint16_t pluginResponseCode = 0;
		if (metadata->responsePoint((void *)&request, &pluginContent, &pluginResponseSize, &pluginResponseCode, &typeResponse, &pluginAdditionalHeader))
		{
			HTTP_sendHeaderResponse(typeResponse, pluginResponseCode, pluginResponseSize, pluginAdditionalHeader, ctx, thisClient->id);
			SweetSocket_sendData(pluginContent, pluginResponseSize, ctx, thisClient->id);
			free(pluginContent);
			free(pluginAdditionalHeader);
			free(typeResponse);
			if (!HTTP_isKeepAlive(data))
				SweetSocket_peerClientClose(ctx, thisClient->id);
			return;
		}
		if (!metadata->isKeepLoaded)
			metadata->shutdownPoint();
	}

	// Verificar se é um diretório
	if (HTTP_isDirectory(request.filePath))
	{
		size_t len = wcslen(request.filePath);
		size_t lenV = wcslen(request.virtualPath);
		if (request.filePath[len - 1] != L'/')
		{
			len += 1;
			request.filePath = realloc(request.filePath, (len + 1) * sizeof(wchar_t));
			if (request.filePath == NULL)
			{
				HTTP_sendErrorResponse(500, L"<h1>500 Internal Server Error</h1><p>Memory allocation failed.</p>", ctx, thisClient->id);
				goto HTTPexit;
			}
			request.filePath[len - 1] = L'/';
			request.filePath[len] = L'\0';
			lenV += 1;
		}

		// Tentar arquivo padrão
		wchar_t *defaultPage = HTTP_findDefaultFile(envolvirment->server.defaultPages, request.filePath);
		if (defaultPage != NULL)
		{
			free(request.filePath);
			request.filePath = defaultPage;
			HTTP_transferData(&request, ctx, thisClient);
			goto HTTPexit;
		}

		// Listar diretório
		if (envolvirment->server.allowDirectoryListing)
		{
			request.virtualPath = request.filePath + (len - lenV);
			HTTP_sendDirectoryListing(&request, ctx, thisClient);
			goto HTTPexit;
		}
	}
	else if (HTTP_isFile(request.filePath))
	{
		// Verificar se é um arquivo
		HTTP_transferData(&request, ctx, thisClient);
		goto HTTPexit;
	}

	// Arquivo não encontrado
	HTTP_sendErrorResponse(404, L"<h1>404 Not Found</h1><p>File not found.</p>", ctx, thisClient->id);

HTTPexit:
	HTTP_requestDestroy(&request);
	if (!HTTP_isKeepAlive(data))
		SweetSocket_peerClientClose(ctx, thisClient->id);
}