#include "http_config.h"
#include "http_plugin.h"
#include "http_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

static wchar_t *HTTP_readLine(FILE *file)
{
    wchar_t *line = NULL;
    wchar_t c;
    uint32_t lineSize = 128;
    uint32_t lineLen = 0;
    line = (wchar_t *)calloc(1, lineSize * sizeof(wchar_t));
    if (line == NULL)
    {
        perror("Failed to allocate memory for line");
        return NULL;
    }

    while (true)
    {
        c = fgetwc(file);
        if (c == WEOF || c == L'\n')
            break;
        if (lineLen == lineSize)
        {
            lineSize *= 2;
            line = (wchar_t *)realloc(line, lineSize * sizeof(wchar_t));
            if (line == NULL)
            {
                perror("Failed to reallocate memory for line");
                return NULL;
            }
        }
        line[lineLen++] = c;
    }

    if (lineLen == 0)
    {
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

static wchar_t *HTTP_splitComma(wchar_t *line, wchar_t **saveptr)
{
    return wcstok(line, L",", saveptr);
}

struct HTTP_server_config HTTP_loadConfig()
{
    struct HTTP_server_config configs;
    memset(&configs, 0, sizeof(struct HTTP_server_config));
    configs.configFile = _wfopen(L"http.conf", L"r");
    if (configs.configFile == NULL)
    {
        perror("Failed to open config file");
        return configs;
    }

    bool inHTTPSection = false;
    wchar_t *saveptr;
    for (wchar_t *line = HTTP_readLine(configs.configFile); line != NULL; line = HTTP_readLine(configs.configFile))
    {
        if (wcscmp(line, L"[HTTP]") == 0)
        {
            inHTTPSection = true;
            free(line);
            continue;
        }
        if (!inHTTPSection || line[0] == L'\n' || line[0] == L'#')
        {
            free(line);
            continue;
        }
        if (inHTTPSection && line[0] == L'[')
        {
            free(line);
            break;
        }
        wchar_t *key = wcstok(line, L"=", &saveptr);
        wchar_t *value = wcstok(NULL, L"=", &saveptr);
        HTTP_trimSpaces(key);
        HTTP_trimSpaces(value);
        saveptr = NULL;
        if (key == NULL || value == NULL)
        {
            free(line);
            continue;
        }
        if (wcscmp(key, L"Ports") == 0)
        {
            while (true)
            {
                wchar_t *port = HTTP_splitComma(saveptr == NULL ? value : NULL, &saveptr);
                if (port == NULL)
                    break;
                uint16_t *portNum = malloc(sizeof(uint16_t));
                if (portNum == NULL)
                {
                    perror("Failed to allocate memory for port number");
                    free(line);
                    return configs;
                }
                *portNum = _wtoi(port);
                HTTP_arrayPush(&configs.ports, portNum);
            }
        }
        else if (wcscmp(key, L"Hosts") == 0)
        {
            while (true)
            {
                wchar_t *host = HTTP_splitComma(saveptr == NULL ? value : NULL, &saveptr);
                if (host == NULL)
                    break;
                wchar_t *hostStr = malloc((wcslen(host) + 1) * sizeof(wchar_t));
                if (hostStr == NULL)
                {
                    perror("Failed to allocate memory for host string");
                    free(line);
                    return configs;
                }
                wcscpy(hostStr, host);
                HTTP_arrayPush(&configs.hosts, hostStr);
            }
        }
        else if (wcscmp(key, L"Root") == 0)
        {
            configs.root = malloc((wcslen(value) + 1) * sizeof(wchar_t));
            if (configs.root == NULL)
            {
                perror("Failed to allocate memory for root");
                free(line);
                return configs;
            }
            wcscpy(configs.root, value);
        }
        else if (wcscmp(key, L"DefaultPages") == 0)
        {
            while (true)
            {
                wchar_t *page = HTTP_splitComma(saveptr == NULL ? value : NULL, &saveptr);
                if (page == NULL)
                    break;
                wchar_t *pageStr = malloc((wcslen(page) + 1) * sizeof(wchar_t));
                if (pageStr == NULL)
                {
                    perror("Failed to allocate memory for default page");
                    free(line);
                    return configs;
                }
                wcscpy(pageStr, page);
                HTTP_arrayPush(&configs.defaultPages, pageStr);
            }
        }
        else if (wcscmp(key, L"LogFile") == 0)
        {
            char *charValue = malloc((wcslen(value) + 1) * sizeof(char));
            if (charValue == NULL)
            {
                perror("Failed to allocate memory for log file path");
                free(line);
                return configs;
            }
            wcstombs(charValue, value, wcslen(value) + 1);
            configs.logFile = fopen(charValue, "a+");
            free(charValue);
        }
        else if (wcscmp(key, L"PartialMaxFileBlock") == 0)
        {
            configs.partialMaxFileBlock = _wtoi(value);
        }
        else if (wcscmp(key, L"AllowDirList") == 0)
        {
            configs.allowDirectoryListing = (wcscmp(value, L"true") == 0);
        }
        free(line);
    }
    return configs;
}

void HTTP_freeConfig(struct HTTP_server_config *config)
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

void HTTP_loadMimeTypes(struct HTTP_server_envolvirment *envolviment)
{
    if (envolviment == NULL || envolviment->server.configFile == NULL)
        return;

    bool inMimeSection = false;
    wchar_t *saveptr;
    fseek(envolviment->server.configFile, 0, SEEK_SET);
    for (wchar_t *line = HTTP_readLine(envolviment->server.configFile); line != NULL; line = HTTP_readLine(envolviment->server.configFile))
    {
        if (wcscmp(line, L"[MIME]") == 0)
        {
            inMimeSection = true;
            free(line);
            continue;
        }
        if (!inMimeSection || line[0] == L'\n' || line[0] == L'#')
        {
            free(line);
            continue;
        }
        if (inMimeSection && line[0] == L'[')
        {
            free(line);
            break;
        }
        wchar_t *key = wcstok(line, L"=", &saveptr);
        wchar_t *value = wcstok(NULL, L"=", &saveptr);
        HTTP_trimSpaces(key);
        HTTP_trimSpaces(value);
        saveptr = NULL;
        if (key == NULL || value == NULL)
        {
            free(line);
            continue;
        }
        wchar_t *keyStr = malloc((wcslen(key) + 1) * sizeof(wchar_t));
        wchar_t *valueStr = malloc((wcslen(value) + 1) * sizeof(wchar_t));
        if (keyStr == NULL || valueStr == NULL)
        {
            perror("Failed to allocate memory for MIME types");
            free(keyStr);
            free(valueStr);
            free(line);
            continue;
        }
        wcscpy(keyStr, key);
        wcscpy(valueStr, value);
        struct HTTP_server_mine_type *mimeType = malloc(sizeof(struct HTTP_server_mine_type));
        if (mimeType == NULL)
        {
            perror("Failed to allocate memory for MIME type");
            free(keyStr);
            free(valueStr);
            free(line);
            continue;
        }
        mimeType->extension = keyStr;
        mimeType->mineType = valueStr;
        HTTP_arrayPush(&envolviment->mimeTypes, mimeType);
        free(line);
    }
}

static enum HTTP_linked_list_actions HTTP_freeMineTypesAction(struct HTTP_object *actual, void *parms, uint64_t count)
{
    (void)count;
    (void)parms;
    struct HTTP_server_mine_type *mimeType = (struct HTTP_server_mine_type *)actual->object;
    free(mimeType->extension);
    free(mimeType->mineType);
    free(mimeType);
    return ARRAY_FREE;
}

void HTTP_freeMimeTypes(struct HTTP_server_envolvirment *envolviment)
{
    HTTP_arrayForEach(&envolviment->mimeTypes, HTTP_freeMineTypesAction, NULL);
    HTTP_arrayClear(&envolviment->mimeTypes);
}

void HTTP_loadPlugins(struct HTTP_server_envolvirment *envolviment)
{
    if (envolviment == NULL || envolviment->server.configFile == NULL)
        return;

    bool inPluginSection = false;
    fseek(envolviment->server.configFile, 0, SEEK_SET);
    for (wchar_t *line = HTTP_readLine(envolviment->server.configFile); line != NULL; line = HTTP_readLine(envolviment->server.configFile))
    {
        if (wcscmp(line, L"[PLUGINS]") == 0)
        {
            inPluginSection = true;
            free(line);
            continue;
        }
        if (!inPluginSection || line[0] == L'\n' || line[0] == L'#')
        {
            free(line);
            continue;
        }
        if (inPluginSection && line[0] == L'[')
        {
            free(line);
            break;
        }
        HTTP_trimSpaces(line);
        const struct HTTP_plugin_metadata *plugin = HTTP_loadPlugin((const wchar_t *)line);
        if (plugin == NULL)
        {
            free(line);
            continue;
        }
        if (plugin->isKeepLoaded)
            plugin->entryPoint();
        HTTP_arrayPush(&envolviment->plugins, (void *)plugin);
        free(line);
    }
}

static enum HTTP_linked_list_actions HTTP_freePluginAction(struct HTTP_object *actual, void *parms, uint64_t count)
{
    (void)count;
    (void)parms;
    const struct HTTP_plugin_metadata *plugin = (const struct HTTP_plugin_metadata *)actual->object;
    HTTP_pluginUnload(plugin);
    return ARRAY_FREE;
}

void HTTP_freePlugins(struct HTTP_server_envolvirment *envolviment)
{
    HTTP_arrayForEach(&envolviment->plugins, HTTP_freePluginAction, NULL);
    HTTP_arrayClear(&envolviment->plugins);
}