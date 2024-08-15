#include "http_config.h"
#include "http_plugin.h"
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

const struct HTTP_plugin_metadata *HTTP_loadPlugin(const wchar_t *pluginPath)
{
	// Carregar a DLL do plugin
	HMODULE hDLL = LoadLibraryW(pluginPath);
	if (hDLL == NULL)
	{
		perror("Failed to load plugin");
		return NULL;
	}

	// Obter o endereço da função getManifest
	FARPROC procAddress = GetProcAddress(hDLL, "getManifest");
	if (procAddress == NULL)
	{
		perror("Failed to get getManifest");
		FreeLibrary(hDLL);
		return NULL;
	}

	// Converter para ponteiro de função
	struct HTTP_plugin_metadata *(*getManifest)(void);
	*(FARPROC *)&getManifest = procAddress;

	// Retornar o manifest do plugin
	const struct HTTP_plugin_metadata *manifest = getManifest();
	manifest->setModule(hDLL);
	return manifest;
}

void HTTP_pluginUnload(const struct HTTP_plugin_metadata *plugin)
{
	if (plugin->shutdownPoint != NULL)
	{
		// Chamar o ponto de desligamento do plugin, se existir
		plugin->shutdownPoint();
	}
	if (plugin->getModule != NULL)
	{
		// Obter o módulo da DLL do plugin
		HMODULE hDLL = plugin->getModule();
		if (hDLL != NULL)
		{
			// Descarregar a DLL do plugin
			FreeLibrary(hDLL);
		}
	}
}
