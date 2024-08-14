#include <http_plugin.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

struct HTTP_plugin_metadata manifest = {
	.isKeepLoaded = true,
	.type = PLUGIN_TYPE_ALL,
	.name = "http_java",
	.entryPoint = NULL,
	.shutdownPoint = NULL,
	.requestPoint = NULL,
	.responsePoint = NULL,
	.setModule = NULL,
	.getModule = NULL
};

struct jvm {
	JavaVM* vm;
	JNIEnv* env;
};

struct javaClass {
	jclass cls;
	jmethodID constructor;
	jfieldID requestSuccessful;
	jfieldID body;
	jfieldID contentType;
	jfieldID headerAppend;
	jfieldID statusCode;
};

struct pluginClass {
	struct jvm jvm;
	struct javaClass javaClass;
};

struct reqestData {
	char* data;
	uint64_t responseSize;
	uint16_t responseCode;
	char* responseType;
	char* adictionalHeader;
};


struct pluginClass* plugin = NULL;
HMODULE g_hModule = NULL;

static bool loadClass() {
	if (plugin == NULL || plugin->jvm.env == NULL) {
		perror("JVM not initialized");
		return false;
	}
	plugin->javaClass.cls = (*plugin->jvm.env)->FindClass(plugin->jvm.env, "WebMain");
	if (plugin->javaClass.cls == NULL) {
		perror("Failed to find class");
		return false;
	}
	plugin->javaClass.constructor = (*plugin->jvm.env)->GetMethodID(plugin->jvm.env, plugin->javaClass.cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;[BI)V");
	if (plugin->javaClass.constructor == NULL) {
		perror("Failed to get constructor");
		return false;
	}
	plugin->javaClass.requestSuccessful = (*plugin->jvm.env)->GetFieldID(plugin->jvm.env, plugin->javaClass.cls, "requestSuccessful", "Z");
	plugin->javaClass.body = (*plugin->jvm.env)->GetFieldID(plugin->jvm.env, plugin->javaClass.cls, "body", "[B");
	plugin->javaClass.contentType = (*plugin->jvm.env)->GetFieldID(plugin->jvm.env, plugin->javaClass.cls, "contentType", "Ljava/lang/String;");
	plugin->javaClass.headerAppend = (*plugin->jvm.env)->GetFieldID(plugin->jvm.env, plugin->javaClass.cls, "headerAppend", "Ljava/lang/String;");
	plugin->javaClass.statusCode = (*plugin->jvm.env)->GetFieldID(plugin->jvm.env, plugin->javaClass.cls, "statusCode", "I");
	if (plugin->javaClass.requestSuccessful == NULL || plugin->javaClass.body == NULL || plugin->javaClass.contentType == NULL || plugin->javaClass.headerAppend == NULL || plugin->javaClass.statusCode == NULL) {
		perror("Failed to get field");
		return false;
	}
	return true;
}

static struct reqestData* request(struct HTTP_request* requestInput) {
	struct reqestData* response = NULL;
	if (plugin == NULL || plugin->jvm.env == NULL) {
		perror("JVM not initialized");
		return NULL;
	}

	// Atraca a thread atual ao JVM necessario pois o java e NTS
	(*plugin->jvm.vm)->AttachCurrentThread(plugin->jvm.vm, (void**)&plugin->jvm.env, NULL);

	// Convertendo os dados para o formato do Java
	jstring jstr = (*plugin->jvm.env)->NewStringUTF(plugin->jvm.env, requestInput->header);
	jstring jstr2 = NULL;
	if (requestInput->getContent != NULL) {
		size_t getContentSize = wcslen(requestInput->getContent);
		char* getContentChar = (char*)malloc(getContentSize + 1);
		wcstombs(getContentChar, requestInput->getContent, getContentSize + 1);
		jstr2 = (*plugin->jvm.env)->NewStringUTF(plugin->jvm.env, getContentChar);
		free(getContentChar);
	}
	jbyteArray jbody = (*plugin->jvm.env)->NewByteArray(plugin->jvm.env, requestInput->dataSize);
	(*plugin->jvm.env)->SetByteArrayRegion(plugin->jvm.env, jbody, 0, requestInput->dataSize, (jbyte*)requestInput->data);
	jint jbodySize = requestInput->dataSize;

	// Criando o objeto Java
	jobject object = (*plugin->jvm.env)->NewObject(plugin->jvm.env, plugin->javaClass.cls, plugin->javaClass.constructor, jstr, jstr2, jbody, jbodySize);
	if (object == NULL) {
		perror("Failed to create object");
		goto cleanup;
	}

	// Checando se a requisição foi bem sucedida
	jboolean requestSuccessful = (*plugin->jvm.env)->GetBooleanField(plugin->jvm.env, object, plugin->javaClass.requestSuccessful);
	if (requestSuccessful == JNI_FALSE) {
		perror("Request failed");
		goto cleanup;
	}

	// Obtendo os dados da resposta
	jarray reciveBody = (jarray)(*plugin->jvm.env)->GetObjectField(plugin->jvm.env, object, plugin->javaClass.body);
	jstring contentType = (jstring)(*plugin->jvm.env)->GetObjectField(plugin->jvm.env, object, plugin->javaClass.contentType);
	jstring headerAppend = (jstring)(*plugin->jvm.env)->GetObjectField(plugin->jvm.env, object, plugin->javaClass.headerAppend);
	jint statusCode = (*plugin->jvm.env)->GetIntField(plugin->jvm.env, object, plugin->javaClass.statusCode);

	if (reciveBody == NULL || contentType == NULL) {
		perror("Failed to get response data");
		goto cleanup;
	}

	// Convertendo os dados de resposta para o formato C
	jsize reciveBodySize = (*plugin->jvm.env)->GetArrayLength(plugin->jvm.env, reciveBody);
	jbyte* bodyData = (*plugin->jvm.env)->GetByteArrayElements(plugin->jvm.env, reciveBody, NULL);
	char* contentTypeData = (*plugin->jvm.env)->GetStringUTFChars(plugin->jvm.env, contentType, NULL);
	char* headerAppendData = (headerAppend != NULL ? (*plugin->jvm.env)->GetStringUTFChars(plugin->jvm.env, headerAppend, NULL) : NULL);

	// Alocando memória para a resposta
	response = (struct reqestData*)malloc(sizeof(struct reqestData));
	if (response == NULL) {
		perror("Failed to allocate memory for response");
		goto cleanup;
	}

	response->responseSize = reciveBodySize;
	response->responseCode = statusCode;
	response->responseType = contentTypeData;
	response->adictionalHeader = headerAppendData;
	response->data = (char*)malloc(reciveBodySize);
	if (response->data == NULL) {
		perror("Failed to allocate memory for response data");
		free(response);
		response = NULL;
		goto cleanup;
	}
	memcpy(response->data, bodyData, reciveBodySize);
cleanup:
	// Liberando recursos
	(*plugin->jvm.vm)->DetachCurrentThread(plugin->jvm.vm);
	return response;
}



__declspec(dllexport) bool responsePoint(struct HTTP_request* requestInput, char** responseContent, uint64_t* responseSize, uint16_t* responseCode, char** responseType, char** adictionalHeader)
{
	if (plugin == NULL) {
		perror("Plugin not initialized");
		return false;
	}
	struct reqestData* response = request(requestInput);
	if (response == NULL) {
		perror("Failed to get response");
		return false;
	}
	*responseSize = response->responseSize;
	*responseCode = response->responseCode;
	*responseType = response->responseType;
	*adictionalHeader = response->adictionalHeader;
	*responseContent = response->data;
	free(response);
	return true;
}

__declspec(dllexport) bool requestPoint(struct HTTP_request* request)
{
	return false;
}

__declspec(dllexport) void entryPoint()
{
	if (plugin != NULL) {
		wprintf(L"Plugin already initialized\n");
		return;
	}

	plugin = (struct pluginClass*)malloc(sizeof(struct pluginClass));
	if (plugin == NULL) {
		wprintf(L"Failed to allocate memory for pluginClass\n");
		return;
	}

	JavaVMOption options[4];
	options[0].optionString = "-agentlib:jdwp=transport=dt_socket,server=y,suspend=n,address=5005";
	options[1].optionString = "-Djava.class.path=D:\\Data\\SweetSocket\\examples\\http\\plugins\\http_plugin_java";
	options[2].optionString = "-Xmx512m";
	options[3].optionString = "-Xms256m";

	JavaVMInitArgs args;
	args.version = JNI_VERSION_21;
	args.nOptions = 4;
	args.options = options;
	args.ignoreUnrecognized = JNI_TRUE;

	jint res = JNI_CreateJavaVM(&plugin->jvm.vm, (void**)&plugin->jvm.env, &args);
	if (res != JNI_OK) {
		wprintf(L"Failed to create Java JVM, error code: %d\n", res);
		free(plugin);
		plugin = NULL;
		return;
	}

	wprintf(L"Java JVM has started\n");

	if (!loadClass()) {
		(*plugin->jvm.vm)->DestroyJavaVM(plugin->jvm.vm);
		free(plugin);
		plugin = NULL;
		return;
	}
}

__declspec(dllexport) void shutdownPoint()
{
	if (plugin == NULL) {
		wprintf(L"Plugin not initialized\n");
		return;
	}

	if (plugin->jvm.vm != NULL) {
		(*plugin->jvm.vm)->DestroyJavaVM(plugin->jvm.vm);
	}

	free(plugin);
	plugin = NULL;
	wprintf(L"Java JVM has stopped\n");
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
	manifest.setModule = setModule;
	manifest.getModule = getModule;
	return &manifest;
}