#include <http_plugin.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

// Definição da classe C++
#ifdef __cplusplus

#include <iostream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>
#include <locale>
#include <codecvt>

class WebMain {
public:
	WebMain() {
		std::cout << "WebMain created" << std::endl;
	}

	bool requestPoint(struct HTTP_request* request) {
		// Implementação
		return false;
	}

	bool responsePoint(struct HTTP_request* request, char** responseContent, uint64_t* responseSize, uint16_t* responseCode, char** responseType, char** additionalHeader) {
		std::wstringstream ss;

		// Construção da resposta HTML
		ss << L"<html>";
		ss << L"<head>";
		ss << L"<title>HTTP Plugin C++</title>";
		ss << L"</head>";
		ss << L"<body>";
		ss << L"<h1>HTTP Plugin C++</h1>";
		ss << L"<p>Request method: " << request->verb << L"</p>";
		ss << L"<p>Request path: " << request->filePath << L"</p>";
		ss << L"<p>Request query: " << request->getContent << L"</p>";
		ss << L"<p>Request user agent: " << request->userAgent << L"</p>";
		ss << L"</body>";
		ss << L"</html>";

		std::wstring utf16Content = ss.str();
		*responseContent = (char*)std::malloc(utf16Content.size() + 1);
		if (*responseContent) {
			std::memcpy(*responseContent, utf16Content.c_str(), utf16Content.size());
			(*responseContent)[utf16Content.size()] = '\0'; // Null-terminate
			*responseSize = utf16Content.size();
			*responseCode = 200;
			*responseType = (char*)std::malloc(20);
			if (*responseType) {
				std::strcpy(*responseType, "text/html; charset=UTF-16");
			}
			return true;
		}
		return false;
	}
	~WebMain() {
		std::cout << "WebMain destroyed" << std::endl;
	}
};

// Wrappers para exposição em C
extern "C" {
	WebMain* WebMain_create() {
		return new WebMain();
	}

	void WebMain_destroy(WebMain* instance) {
		delete instance;
	}

	bool WebMain_requestPoint(WebMain* instance, struct HTTP_request* request) {
		if (instance) {
			return instance->requestPoint(request);
		}
	}

	bool WebMain_responsePoint(WebMain* instance, struct HTTP_request* request, char** responseContent, uint64_t* responseSize, uint16_t* responseCode, char** responseType, char** additionalHeader) {
		if (instance) {
			return instance->responsePoint(request, responseContent, responseSize, responseCode, responseType, additionalHeader);
		}
	}
}
#endif // Fim da definição da classe e wrappers C++


// Encapsule tudo em um bloco 'extern "C"' se estiver usando C++
#ifdef __cplusplus
extern "C" {
#endif

	// Estrutura de metadados do plugin
	struct HTTP_plugin_metadata manifest = {
		true,
		PLUGIN_TYPE_ALL,
		"http_cpp",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	};

	// Variável global para armazenar o módulo
	HMODULE g_hModule = NULL;
	WebMain* webMain = NULL;

	// Implementação dos pontos de entrada e saída do plugin
	__declspec(dllexport) bool responsePoint(struct HTTP_request* requestInput, char** responseContent, uint64_t* responseSize, uint16_t* responseCode, char** responseType, char** adictionalHeader)
	{
		if (webMain) {
			return WebMain_responsePoint(webMain, requestInput, responseContent, responseSize, responseCode, responseType, adictionalHeader);
		}
		return false;
	}

	__declspec(dllexport) bool requestPoint(struct HTTP_request* request)
	{
		if (webMain) {
			return WebMain_requestPoint(webMain, request);
		}
		return false;
	}

	__declspec(dllexport) void entryPoint()
	{
		webMain = WebMain_create();
	}

	__declspec(dllexport) void shutdownPoint()
	{
		if (webMain) {
			WebMain_destroy(webMain);
		}
	}

	__declspec(dllexport) void setModule(HMODULE module) {
		g_hModule = module;
	}

	__declspec(dllexport) HMODULE getModule() {
		return g_hModule;
	}

	__declspec(dllexport) const struct HTTP_plugin_metadata* getManifest()
	{
		manifest.requestPoint = requestPoint;
		manifest.responsePoint = responsePoint;
		manifest.entryPoint = entryPoint;
		manifest.shutdownPoint = shutdownPoint;
		manifest.setModule = reinterpret_cast<void (*)(void*)>(setModule);
		manifest.getModule = reinterpret_cast<void* (*)(void)>(getModule);
		return &manifest;
	}

#ifdef __cplusplus
} // Fim do bloco 'extern "C"'
#endif