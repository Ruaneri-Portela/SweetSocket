#include "http_config.h"
#include "http_plugin.h"
#include "http_process.h"
#include "http_utils.h"
#include <SweetSocket.h>

static void HTTP_transferData(struct HTTPsendParms* parms, struct socketGlobalContext* context, struct socketClients* thisClient) {
	char* options = NULL;
	char* type = NULL;
	uint16_t command = 200;

	HANDLE hFile = CreateFileW(parms->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		HTTP_sendErrorResponse(404, L"<h1>404 Not Found</h1><p>File not found.</p>", context, thisClient->id);
		return;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(hFile, &fileSize))
	{
		CloseHandle(hFile);
		HTTP_sendErrorResponse(500, L"<h1>500 Internal Server Error</h1><p>Cannot get file size.</p>", context, thisClient->id);
		return;
	}

	// Verificar se � um range (parcial)
	if (parms->requestedStartFile != -1 || parms->requestedEndFile != -1)
	{
		const char* format = "Accept-Ranges: bytes\r\nContent-Range: bytes %lld-%lld/%lld\r\n";
		options = (char*)malloc(100);
		int64_t physicalFileSize = fileSize.QuadPart;
		parms->requestedEndFile = (parms->requestedEndFile == -1) ? fileSize.QuadPart - 1 : parms->requestedEndFile;
		snprintf(options, 100,
			format,
			parms->requestedStartFile,
			parms->requestedEndFile,
			fileSize.QuadPart);
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

	// Enviar cabe�alho
	type = HTTP_getMimeType(parms->path, &parms->envolvirment->mimeTypes);
	HTTP_sendHeaderResponse(type, command, fileSize.QuadPart, options, context, thisClient->id);
	if (type != NULL)
		free(type);
	if (options != NULL)
		free(options);

	// Enviar arquivo
	int64_t sent = (fileSize.QuadPart > parms->envolvirment->server.partialMaxFileBlock) ? parms->envolvirment->server.partialMaxFileBlock : fileSize.QuadPart;
	char* fileData = (char*)malloc(sent);
	if (fileData == NULL)
	{
		CloseHandle(hFile);
		HTTP_sendErrorResponse(500, L"<h1>500 Internal Server Error</h1><p>Cannot allocate memory.</p>", context, thisClient->id);
		return;
	}

	for (int64_t totalSent = 0; totalSent < fileSize.QuadPart;) {
		int64_t toRead = (fileSize.QuadPart - totalSent > parms->envolvirment->server.partialMaxFileBlock) ? parms->envolvirment->server.partialMaxFileBlock : (size_t)(fileSize.QuadPart - totalSent);
		DWORD readSize;
		if (ReadFile(hFile, fileData, toRead, &readSize, NULL))
		{
			if (readSize > 0)
			{
				if (!sendData(fileData, readSize, context, thisClient->id))
				{
					closeClient(context, thisClient->id);
					break;
				}
				totalSent += readSize;
				continue;
			}
		}
	}
	CloseHandle(hFile);
	free(fileData);
	return;
}

static void HTTP_sendDirectoryListing(struct HTTPsendParms* parms, struct socketGlobalContext* context, struct socketClients* thisClient) {
	if (parms->envolvirment->server.allowDirectoryListing == false)
	{
		HTTP_sendErrorResponse(403, L"<h1>403 Forbidden</h1><p>Directory listing not allowed.</p>", context, thisClient->id);
		return;
	}
	wchar_t* html = NULL;
	size_t htmlSize = HTTP_createHtmlDirectoryList(parms->path, parms->virtualPath, &html);
	if (htmlSize == 0)
	{
		HTTP_sendErrorResponse(404, L"<h1>404 Not Found</h1><p>Directory not found.</p>", context, thisClient->id);
		return;
	}
	HTTP_sendHeaderResponse("text/html; charset=UTF-16", 200, htmlSize, NULL, context, thisClient->id);
	sendData((const char*)html, htmlSize, context, thisClient->id);
	free(html);
	return;
}

void HTTP_processClientRequest(char* data, uint64_t size, struct socketGlobalContext* ctx, struct socketClients* thisClient, void* parms)
{
	data[size] = '\0';
	struct HTTP_server_envolvirment* envolvirment = (struct HTTP_server_envolvirment*)parms;
	wchar_t* virtualPath = NULL;
	wchar_t* realPath = HTTP_getRequestPath(data, envolvirment->server.root, &virtualPath);
	char* userAgent = HTTP_getUserAgent(data, size);
	char* verb = HTTP_getVerb(data, size);
	char* host = HTTP_getHost(data, size);
	bool exit = false;


	HTTP_logClientRequest(&envolvirment->server, verb, virtualPath, userAgent == NULL ? "" : userAgent, (void *)thisClient);

	for (struct HTTP_object* plugin = envolvirment->plugins.base; plugin != NULL; plugin->next)
	{
		struct HTTP_plugin_metadata* metadata = (struct HTTP_plugin_metadata*)plugin->object;
		if (!metadata->isKeepLoaded)
			metadata->entryPoint();

		char* pluginContent = NULL;
		uint64_t pluginResponseSize = 0;
		uint16_t pluginResponseCode	= 0;
		char* pluginAdcionalHeader	= NULL;
		if (metadata->responsePoint( data, &pluginContent, &pluginResponseSize, &pluginResponseCode, &pluginAdcionalHeader)) {
			HTTP_sendHeaderResponse("text/html", pluginResponseCode, pluginResponseSize, pluginAdcionalHeader, ctx, thisClient->id);
			sendData(pluginContent, pluginResponseSize, ctx, thisClient->id);
			exit = HTTP_isKeepAlive(data);
			return;
		}
		if (!metadata->isKeepLoaded)
			metadata->shutdownPoint();
	}

	struct HTTPsendParms sendParms = { realPath, virtualPath,-1, -1, envolvirment };
	// Verificar se � um diret�rio
	if (HTTP_isDirectory(realPath))
	{
		size_t len = wcslen(realPath);
		size_t lenV = wcslen(virtualPath);
		if (realPath[len] != L'/')
		{
			len += 1;
			realPath = realloc(realPath, (len + 1) * sizeof(wchar_t));
			realPath[len - 1] = L'/';
			realPath[len] = L'\0';
			lenV += 1;
		}

		// Tentar arquivo padr�o
		wchar_t* defaultPage = HTTP_findDefaultFile(envolvirment->server.defaultPages, realPath);
		if (defaultPage != NULL)
		{
			free(realPath);
			realPath = defaultPage;
			sendParms.path = realPath;
			goto HTTPsend;
		}
		// Listar diret�rio
		if (envolvirment->server.allowDirectoryListing == true) {
			virtualPath = realPath + (len - lenV);
			sendParms.virtualPath = virtualPath;
			sendParms.path = realPath;
			HTTP_sendDirectoryListing(&sendParms, ctx, thisClient);
			goto HTTPexit;
		}
		goto HTTPexit;
	}
	// Verificar se � um arquivo
	if (HTTP_isFile(realPath))
	{
	HTTPsend:
		HTTP_getRangeValues(data, &sendParms.requestedStartFile, &sendParms.requestedEndFile);
		HTTP_transferData(&sendParms, ctx, thisClient);
		goto HTTPexit;
	}
	// Arquivo n�o encontrado

	HTTP_sendErrorResponse(404, L"<h1>404 Not Found</h1><p>File not found.</p>", ctx, thisClient->id);
HTTPexit:
	exit = HTTP_isKeepAlive(data);
	free(userAgent);
	free(host);
	free(verb);
	free(realPath);
	if (exit == false)
		closeClient(ctx, thisClient->id);
}