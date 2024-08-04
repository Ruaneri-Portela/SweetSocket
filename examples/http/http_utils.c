#include "http.h"

struct HTTPServerEnv HTTP_loadConf()
{
    struct HTTPServerEnv env;
    memset(&env, 0, sizeof(env));
    env.configFile = (char *)malloc(PATH_MAX);
    GetFullPathName("http.conf", PATH_MAX, env.configFile, NULL);
    FILE *file = fopen(env.configFile, "r");
    if (file == NULL)
    {
        printf("Arquivo de configuração não encontrado.\n");
        exit(1);
    }
    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        if (line[0] == '#')
            continue;
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        if (value != NULL)
        {
            HTTP_trim(value);
        }
        if (strcmp(key, "Port") == 0)
        {
            env.port = atoi(value);
        }
        else if (strcmp(key, "Root") == 0)
        {
            _chdir(value);
            char cwd[1024];
            _getcwd(cwd, sizeof(cwd));
            env.root = malloc(strlen(cwd) + 1);
            strcpy(env.root, cwd);
        }
        else if (strcmp(key, "Hosts") == 0)
        {
            env.hosts = malloc(strlen(value) + 1);
            strcpy(env.hosts, value);
        }
        else if (strcmp(key, "DefaultPage") == 0)
        {
            env.defaultPage = malloc(strlen(value) + 1);
            strcpy(env.defaultPage, value);
        }
        else if (strcmp(key, "AllowDirList") == 0)
        {
            env.allowDirectoryListing = (strcmp(value, "true") == 0);
        }
        else if (strcmp(key, "LogFile") == 0)
        {
            env.logFile = malloc(strlen(value) + 1);
            strcpy(env.logFile, value);
        }
    }
    fclose(file);
    return env;
}

bool HTTP_isDirectory(const char *path)
{
    DWORD dwAttrib = GetFileAttributesA(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool HTTP_isFile(const char *path)
{
    DWORD dwAttrib = GetFileAttributesA(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void HTTP_trim(char *str)
{
    char *end;

    while (isspace((unsigned char)*str))
        str++;

    if (*str == 0)
        return;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    *(end + 1) = '\0';
}

void HTTP_getTimeStr(char *buffer, size_t bufferSize)
{
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);

    timeinfo = localtime(&rawtime);

    strftime(buffer, bufferSize, "%d:%m:%Y %H:%M:%S", timeinfo);
}

void HTTP_extractPath(const char *request, char *path, size_t path_size)
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

void HTTP_extractRange(const char *request, int64_t *start, int64_t *end)
{
    *start = -1;
    *end = -1;
    const char *range = strstr(request, "Range: bytes=");
    if (range == NULL)
        return;
    const char *rangeEnd = strchr(range, '\r');
    if (rangeEnd == NULL)
        return;
    const char *firstValue = range + 13;
    const char *dash = strchr(firstValue, '-');
    if (dash == NULL)
        return;
    const char *secondValue = dash + 1;
    if (secondValue < rangeEnd)
        *end = strtoll(secondValue, NULL, 10);
    *start = strtoll(firstValue, NULL, 10);
}

void HTTP_logRequest(struct HTTPServerEnv *server, const char *verb, const char *path, struct socketClients *client)
{
    if (server->logFile == NULL)
    {
        return;
    }
    if (client->client->addr == NULL)
    {
        resolvePeer(client);
    }
    FILE *file = fopen(server->logFile, "a");
    if (file == NULL)
    {
        return;
    }
    char *data = (char *)malloc(20);
    HTTP_getTimeStr(data, 20);
    fprintf(file, "(%s) %s->[%d] %s %s\n", data, client->client->addr, client->client->port, verb, path);
    free(data);
    fclose(file);
}

const char *HTTP_mineType(const char *path, struct HTTPServerEnv *server)
{
    FILE *file = fopen(server->configFile, "r");
    const char *noneMine = "application/octet-stream";
    if (file == NULL)
    {
        return noneMine;
    }

    char line[256];
    size_t fileNameSize = strlen(path);
    char *fileName = malloc(fileNameSize + 1);

    memcpy(fileName, path, fileNameSize);
    fileName[fileNameSize] = '\0';
    char *name = strtok(fileName, ".");
    char *extension = strtok(NULL, ".");

    bool mineSector = false;
    while (fgets(line, sizeof(line), file))
    {
        if (line[0] == '#')
            continue;
        if (line[0] == '[')
        {
            HTTP_trim(line);
            if (!mineSector && strcmp(line, "[MIME]\n") == 1)
            {
                mineSector = true;
                continue;
            }
            if (mineSector && line[0] == '[')
            {
                break;
            }
        }
        if (!mineSector)
        {
            continue;
        }
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        if (value != NULL)
        {
            HTTP_trim(value);
        }
        if (strcmp(extension, key) == 0)
        {
            fclose(file);
            return value;
        }
    }
    fclose(file);
    return noneMine;
}

char *HTTP_getVerb(const char *request, size_t size)
{
    const char *end = strchr(request, ' ');
    if (end == NULL)
    {
        return NULL;
    }
    size_t length = end - request;
    char *verb = (char *)malloc(length + 1);
    strncpy(verb, request, length);
    verb[length] = '\0';
    return verb;
}

size_t HTTP_htmlListDir(const char *path, char **html)
{
    const char *htmlDocument = "<!DOCTYPE html><html><head><meta charset=\"US-ASCII\"><title>%s</title></head><body><h1>Files</h1><ul>%s</ul></body></html>";
    const char *listItem = "<li><a href=\"%s%s\">%s</a></li>";
    char *dirPhysicalPath = NULL;
    char *cleanPath = NULL;
    char *data = NULL;
    size_t pathLen = strlen(path);
    size_t dataSize = 0;
    size_t listItemSize = strlen(listItem);
    size_t cleanPathSize = 0;
    if (path[strlen(path) - 1] != '/')
    {
        dirPhysicalPath = (char *)malloc(pathLen + 3);
        memcpy(dirPhysicalPath, path, pathLen);
        memcpy(dirPhysicalPath + pathLen, "/*", 3);
        cleanPathSize = pathLen;
    }
    else
    {
        dirPhysicalPath = (char *)malloc(pathLen + 2);
        memcpy(dirPhysicalPath, path, pathLen);
        memcpy(dirPhysicalPath + pathLen, "*", 2);
        cleanPathSize = (pathLen - 1);
    }

    pathLen = strlen(dirPhysicalPath);
    cleanPath = (char *)malloc(cleanPathSize + 1);
    memcpy(cleanPath, dirPhysicalPath + 1, cleanPathSize);
    cleanPath[cleanPathSize] = '\0';

    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA(dirPhysicalPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        free(dirPhysicalPath);
        return 0;
    }
    do
    {
        char *name = findFileData.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        // Verificar se é diretório
        size_t nameSize = strlen(name);
        char *temp = (char *)malloc(nameSize + pathLen);
        memcpy(temp, dirPhysicalPath, pathLen - 1);
        memcpy(temp + pathLen - 1, name, nameSize + 1);
        if (HTTP_isDirectory(temp))
        {
            name = (char *)malloc(strlen(findFileData.cFileName) + 2);
            strcpy(name, findFileData.cFileName);
            strcat(name, "/");
        }

        size_t size = (strlen(name) * 2) + listItemSize + cleanPathSize;
        char *newData = (char *)malloc(size);
        snprintf(newData, size, listItem, cleanPath, name, name);
        if (data == NULL)
        {
            dataSize = size;
            data = newData;
            continue;
        }
        dataSize -= 6;
        data = (char *)realloc(data, dataSize + size);
        memcpy(data + dataSize, newData, size);
        dataSize += size;
        free(newData);
        if (name != findFileData.cFileName)
        {
            free(name);
        }
    } while (FindNextFileA(hFind, &findFileData) != 0);

    dataSize = dataSize + strlen(htmlDocument) + cleanPathSize + 1;
    *html = (char *)malloc(dataSize);
    snprintf(*html, dataSize, htmlDocument, cleanPath, data);
    (*html)[dataSize - 2] = '\0';
    FindClose(hFind);
    free(data);
    free(dirPhysicalPath);
    return strlen(*html);
}