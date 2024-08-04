#include <SweetSocket.h>
#define PATHSIZE 512
#define MAXFILESIZE 524288
#define HEADER_SIZE 512
#include <ctype.h>
#include <direct.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct HTTPServerEnv
{
    char *root;
    int port;
    char *hosts;
    char *defaultPage;
    char *logFile;
    char *configFile;
    bool allowDirectoryListing;
};

struct HTTPServerEnv HTTP_loadConf();

bool HTTP_isDirectory(const char *path);

bool HTTP_isFile(const char *path);

void HTTP_trim(char *str);

void HTTP_getTimeStr(char *buffer, size_t bufferSize);

void HTTP_extractPath(const char *request, char *path, size_t path_size);

void HTTP_extractRange(const char *request, int64_t *start, int64_t *end);

void HTTP_logRequest(struct HTTPServerEnv *server, const char *verb, const char *path, struct socketClients *client);

const char *HTTP_mineType(const char *path, struct HTTPServerEnv *server);

char *HTTP_getVerb(const char *request, size_t size);

size_t HTTP_htmlListDir(const char *path, char **html);
