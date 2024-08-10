#include "http_config.h"
#include "http_plugin.h"
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

const struct HTTP_plugin_metadata* HTTP_loadPlugin(const wchar_t* pluginPath)
{
	HMODULE hDLL = LoadLibraryW(pluginPath);
	if (hDLL == NULL)
	{
		perror("Failed to load plugin");
		return NULL;
	}
	struct HTTP_plugin_metadata* (*getManifest)(void) = (struct HTTP_plugin_metadata *(*)(void))GetProcAddress(hDLL, "getManifest");
	if (getManifest == NULL)
	{
		perror("Failed to get getManifest");
		FreeLibrary(hDLL);
		return NULL;
	}
	return getManifest();
}

void HTTP_pluginUnload(const struct HTTP_plugin_metadata* plugin)
{
	if (plugin->shutdownPoint != NULL)
		plugin->shutdownPoint();
}