#include "http_config.h"
#include "http_plugin.h"
#include "http_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

static wchar_t* HTTP_readLine(FILE* file) {
	wchar_t* line = NULL;
	wchar_t c;
	uint32_t lineSize = 128;
	uint32_t lineLen = 0;
	line = (wchar_t*)calloc(1, lineSize * sizeof(wchar_t));
	bool endOfFile = false;
	while (true) {
		c = fgetwc(file);
		if (c == WEOF || c == L'\n')
			break;
		if (lineLen == lineSize) {
			lineSize *= 2;
			line = (wchar_t*)realloc(line, lineSize * sizeof(wchar_t));
		}
		line[lineLen++] = c;
	}
	if (lineLen == 0) {
		if (c == WEOF)
		{
			free(line);
			return NULL;
		}
		line[0] = L'\n';
		return line;
	}
	line[lineLen] = L'\0';
	return line;
}

static wchar_t* HTTP_splitComma(wchar_t* line, wchar_t** saveptr)
{
	wchar_t* command = wcstok(line, L",", saveptr);
	if (command == NULL)
		return NULL;
	return command;
}

struct HTTP_server_config HTTP_loadConfig()
{
	struct HTTP_server_config configs;
	memset(&configs, 0, sizeof(struct HTTP_server_config));
	configs.configFile = _wfopen(L"http.conf", L"r");
	wchar_t* saveptr;
	if (configs.configFile == NULL)
	{
		perror("Failed to open config file");
		return configs;
	}
	bool inHTTPSection = false;
	for (wchar_t* line = HTTP_readLine(configs.configFile); line != NULL; line = HTTP_readLine(configs.configFile))
	{
		if (wcscmp(line, L"[HTTP]") == 0)
		{
			inHTTPSection = true;
			goto lineFree;
		}
		if (!inHTTPSection || line[0] == L'\n' || line[0] == L'#')
			goto lineFree;
		if (inHTTPSection && line[0] == L'[') {
			free(line);
			break;
		}
		wchar_t* key = wcstok(line, L"=", &saveptr);
		wchar_t* value = wcstok(NULL, L"=", &saveptr);
		HTTP_trimSpaces(key);
		HTTP_trimSpaces(value);
		saveptr = NULL;
		if (key == NULL || value == NULL)
			goto lineFree;
		if (wcscmp(key, L"Ports") == 0) {
			while (true) {
				wchar_t* port = HTTP_splitComma(saveptr == NULL ? value : NULL, &saveptr);
				if (port == NULL)
					break;
				uint16_t* portNum = malloc(sizeof(uint16_t));
				*portNum = _wtoi(port);
				HTTP_arrayPush(&configs.ports, portNum);
			}
		}
		else if (wcscmp(key, L"Hosts") == 0) {
			while (true) {
				wchar_t* host = HTTP_splitComma(saveptr == NULL ? value : NULL, &saveptr);
				if (host == NULL)
					break;
				wchar_t* hostStr = malloc((wcslen(host) + 1) * sizeof(wchar_t));
				wcscpy(hostStr, host);
				HTTP_arrayPush(&configs.hosts, hostStr);
			}
		}
		else if (wcscmp(key, L"Root") == 0) {
			configs.root = malloc((wcslen(value) + 1) * sizeof(wchar_t));
			wcscpy(configs.root, value);
		}
		else if (wcscmp(key, L"DefaultPages") == 0) {
			while (true) {
				wchar_t* page = HTTP_splitComma(saveptr == NULL ? value : NULL, &saveptr);
				if (page == NULL)
					break;
				wchar_t* pageStr = malloc((wcslen(page) + 1) * sizeof(wchar_t));
				wcscpy(pageStr, page);
				HTTP_arrayPush(&configs.defaultPages, pageStr);
			}
		}
		else if (wcscmp(key, L"LogFile") == 0) {
			char * charValue = malloc((wcslen(value) + 1) * sizeof(char));
			wcstombs(charValue, value, wcslen(value) + 1);
			configs.logFile = fopen(charValue, "a+");
			free(charValue);
		}
		else if (wcscmp(key, L"PartialMaxFileBlock") == 0) {
			configs.partialMaxFileBlock = _wtoi(value);
		}
		else if (wcscmp(key, L"AllowDirList") == 0) {
			configs.allowDirectoryListing = (wcscmp(value, L"true") == 0);
		}
	lineFree:
		free(line);
	}
	return configs;
}

void HTTP_freeConfig(struct HTTP_server_config* config)
{
	if (config->configFile != NULL)
		fclose(config->configFile);
	if (config->logFile != NULL)
		fclose(config->logFile);
	HTTP_arrayClear(&config->ports);
	HTTP_arrayClear(&config->hosts);
	HTTP_arrayClear(&config->defaultPages);
	free(config->root);
}

void HTTP_loadMimeTypes(struct HTTP_server_envolvirment* envolviment) {
	if (envolviment == NULL || envolviment->server.configFile == NULL)
		return;
	bool inMimeSection = false;
	wchar_t* saveptr;
	fseek(envolviment->server.configFile, 0, SEEK_SET);
	for (wchar_t* line = HTTP_readLine(envolviment->server.configFile); line != NULL; line = HTTP_readLine(envolviment->server.configFile))
	{
		if (wcscmp(line, L"[MIME]") == 0)
		{
			inMimeSection = true;
			free(line);
			continue;
		}
		if (!inMimeSection || line[0] == L'\n' || line[0] == L'#') {
			free(line);
			continue;
		}
		if (inMimeSection && line[0] == L'[') {
			free(line);
			break;
		}
		wchar_t* key = wcstok(line, L"=", &saveptr);
		wchar_t* value = wcstok(NULL, L"=", &saveptr);
		HTTP_trimSpaces(key);
		HTTP_trimSpaces(value);
		saveptr = NULL;
		if (key == NULL || value == NULL)
			continue;
		wchar_t* keyStr = malloc((wcslen(key) + 1) * sizeof(wchar_t));
		wcscpy(keyStr, key);
		wchar_t* valueStr = malloc((wcslen(value) + 1) * sizeof(wchar_t));
		wcscpy(valueStr, value);
		struct HTTP_server_mine_type* mineType = malloc(sizeof(struct HTTP_server_mine_type));
		mineType->extension = keyStr;
		mineType->mineType = valueStr;
		HTTP_arrayPush(&envolviment->mimeTypes, mineType);
		free(line);
	}
}

static enum HTTP_linked_list_actions HTTP_freeMineTypesAction(struct HTTP_object* actual, void* parms, uint64_t count) {
	struct HTTP_server_mine_type* mineType = (struct HTTP_server_mine_type*)actual->object;
	free(mineType->extension);
	free(mineType->mineType);
	free(mineType);
	return ARRAY_FREE;
}

void HTTP_freeMimeTypes(struct HTTP_server_envolvirment* envolviment) {
	HTTP_arrayForEach(&envolviment->mimeTypes, HTTP_freeMineTypesAction, NULL);
	HTTP_arrayClear(&envolviment->mimeTypes);
}

void HTTP_loadPlugins(struct HTTP_server_envolvirment* envolviment) {
	if (envolviment == NULL || envolviment->server.configFile == NULL)
		return;
	bool inPluginSection = false;
	fseek(envolviment->server.configFile, 0, SEEK_SET);
	for (wchar_t* line = HTTP_readLine(envolviment->server.configFile); line != NULL; line = HTTP_readLine(envolviment->server.configFile))
	{
		if (wcscmp(line, L"[PLUGINS]") == 0)
		{
			inPluginSection = true;
			free(line);
			continue;
		}
		if (!inPluginSection || line[0] == L'\n' || line[0] == L'#') {
			free(line);
			continue;
		}
		if (inPluginSection && line[0] == L'[') {
			free(line);
			break;
		}
		HTTP_trimSpaces(line);
		struct HTTP_plugin_metadata* plugin = HTTP_loadPlugin(line);
		if (plugin == NULL)
			continue;
		if (plugin->isKeepLoaded)
			plugin->entryPoint();
		HTTP_arrayPush(&envolviment->plugins, plugin);
		free(line);
	}
}

static enum HTTP_linked_list_actions HTTP_freePluginAction(struct HTTP_object* actual, void* parms, uint64_t count) {
	const struct HTTP_plugin_metadata* plugin = (const struct HTTP_plugin_metadata*)actual->object;
	HTTP_pluginUnload(plugin);
	return ARRAY_FREE;
}

void HTTP_freePlugins(struct HTTP_server_envolvirment* envolviment) {
	HTTP_arrayForEach(&envolviment->plugins, HTTP_freePluginAction, NULL);
	HTTP_arrayClear(&envolviment->plugins);
}