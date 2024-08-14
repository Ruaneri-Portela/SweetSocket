#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "http_array.h"

#define MAXFILESIZE 524288

struct HTTP_server_config
{
	wchar_t *root;
	struct HTTP_linked_list ports;
	struct HTTP_linked_list hosts;
	struct HTTP_linked_list defaultPages;
	struct HTTP_linked_list mimeTypes;
	FILE *logFile;
	FILE *configFile;
	uint32_t partialMaxFileBlock;
	bool allowDirectoryListing;
};

struct HTTP_server_envolvirment
{
	struct HTTP_server_config server;
	struct SweetSocket_global_context *context;
	struct HTTP_linked_list plugins;
	struct HTTP_linked_list virtualHosts;
	struct HTTP_linked_list mimeTypes;
};

struct HTTP_server_mine_type
{
	wchar_t *extension;
	wchar_t *mineType;
};

struct HTTP_server_config HTTP_loadConfig();

void HTTP_freeConfig(struct HTTP_server_config *config);

void HTTP_loadMimeTypes(struct HTTP_server_envolvirment *envolviment);

void HTTP_freeMimeTypes(struct HTTP_server_envolvirment *envolviment);

void HTTP_loadPlugins(struct HTTP_server_envolvirment *envolviment);

void HTTP_freePlugins(struct HTTP_server_envolvirment *envolviment);