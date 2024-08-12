#include "http_config.h"
#include "http_plugin.h"
#include "http_process.h"
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

/**
 * Carrega um plugin a partir de um caminho fornecido.
 *
 * @param pluginPath Caminho para o arquivo DLL do plugin.
 * @return Ponteiro para a estrutura `HTTP_plugin_metadata` obtida a partir do plugin, ou NULL em caso de erro.
 */
const struct HTTP_plugin_metadata* HTTP_loadPlugin(const wchar_t* pluginPath) {
	// Carregar a DLL do plugin
	HMODULE hDLL = LoadLibraryW(pluginPath);
	if (hDLL == NULL) {
		perror("Failed to load plugin");
		return NULL;
	}

	// Obter o endereço da função getManifest
	struct HTTP_plugin_metadata* (*getManifest)(void) = (struct HTTP_plugin_metadata* (*)(void))GetProcAddress(hDLL, "getManifest");
	if (getManifest == NULL) {
		perror("Failed to get getManifest");
		FreeLibrary(hDLL);
		return NULL;
	}
	// Retornar o manifest do plugin
	const struct HTTP_plugin_metadata* manifest = getManifest();
	manifest->setModule(hDLL);
	return manifest;
}

/**
 * Libera os recursos associados a um plugin.
 *
 * @param plugin Ponteiro para a estrutura `HTTP_plugin_metadata` do plugin a ser descarregado.
 */
void HTTP_pluginUnload(const struct HTTP_plugin_metadata* plugin) {
	if (plugin->shutdownPoint != NULL) {
		// Chamar o ponto de desligamento do plugin, se existir
		plugin->shutdownPoint();
	}
	if (plugin->getModule != NULL) {
		// Obter o módulo da DLL do plugin
		HMODULE hDLL = plugin->getModule();
		if (hDLL != NULL) {
			// Descarregar a DLL do plugin
			FreeLibrary(hDLL);
		}
	}
}
