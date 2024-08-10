#include <stdbool.h>
#include <stdint.h>

enum HTTP_plugin_type
{
	PLUGIN_TYPE_ALL,
	PLUGIN_TYPE_REQUEST,
	PLUGIN_TYPE_RESPONSE,
};

struct HTTP_plugin_metadata
{
	bool isKeepLoaded;
	enum HTTP_plugin_type type;
	const char* name;
	void (*entryPoint)();
	void (*shutdownPoint)();
	bool (*requestPoint)(void* pluginClass, char** headerRequest);
	bool (*responsePoint)(const char* headerRequest, char** bodyResponse, uint64_t* responseSize, uint16_t* responseCode,char** adictionalHeader);
};

const struct HTTP_plugin_metadata* HTTP_loadPlugin(const wchar_t* pluginPath);

void HTTP_pluginUnload(const struct HTTP_plugin_metadata* plugin);