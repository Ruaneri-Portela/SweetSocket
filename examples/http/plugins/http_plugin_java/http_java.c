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
	.responsePoint = NULL
};

struct jvm {
	JavaVM* vm;
	JNIEnv* env;
};

struct javaClass {
	jclass cls;
	jmethodID constructor;
	jmethodID returnBodyMethod;
	jmethodID returnRequestSuccessfulMethod;
};

struct pluginClass {
	struct jvm jvm;
	struct javaClass javaClass;
};

struct pluginClass* plugin = NULL;

static bool loadClass() {
	if (plugin == NULL || plugin->jvm.env == NULL) {
		wprintf(L"JVM not initialized\n");
		return false;
	}

	plugin->javaClass.cls = (*plugin->jvm.env)->FindClass(plugin->jvm.env, "WebMain");
	if (plugin->javaClass.cls == NULL) {
		wprintf(L"Failed to find class\n");
		return false;
	}
	plugin->javaClass.constructor = (*plugin->jvm.env)->GetMethodID(plugin->jvm.env, plugin->javaClass.cls, "<init>", "(Ljava/lang/String;)V");
	if (plugin->javaClass.constructor == NULL) {
		wprintf(L"Failed to find constructor\n");
		return false;
	}
	plugin->javaClass.returnBodyMethod = (*plugin->jvm.env)->GetMethodID(plugin->jvm.env, plugin->javaClass.cls, "returnBody", "()[B");
	if (plugin->javaClass.returnBodyMethod == NULL) {
		wprintf(L"Failed to find returnBody method\n");
		return false;
	}
	plugin->javaClass.returnRequestSuccessfulMethod = (*plugin->jvm.env)->GetMethodID(plugin->jvm.env, plugin->javaClass.cls, "returnRequestSuccessful", "()Z");
	if (plugin->javaClass.returnRequestSuccessfulMethod == NULL) {
		wprintf(L"Failed to find returnRequestSuccessful method\n");
		return false;
	}
	return true;
}

static void request(const char * header) {
	jstring jstr = (*plugin->jvm.env)->NewStringUTF(plugin->jvm.env, header);
	jobject obj = (*plugin->jvm.env)->NewObject(plugin->jvm.env, plugin->javaClass.cls, plugin->javaClass.constructor, jstr);
	if (obj == NULL) {
		wprintf(L"Failed to create object\n");
		return;
	}
	jboolean success = (*plugin->jvm.env)->CallBooleanMethod(plugin->jvm.env, obj, plugin->javaClass.returnRequestSuccessfulMethod);
	if (success == JNI_FALSE) {
		wprintf(L"Failed to call returnRequestSuccessful method\n");
		return;
	}
	jbyteArray body = (jbyteArray)(*plugin->jvm.env)->CallObjectMethod(plugin->jvm.env, obj, plugin->javaClass.returnBodyMethod);
	if (body == NULL) {
		wprintf(L"Failed to call returnBody method\n");
		return;
	}
	jsize len = (*plugin->jvm.env)->GetArrayLength(plugin->jvm.env, body);
	jbyte* bodyData = (*plugin->jvm.env)->GetByteArrayElements(plugin->jvm.env, body, NULL);
	if (bodyData == NULL) {
		wprintf(L"Failed to get body data\n");
		return;
	}
	char* bodyDataChar = (const char*)bodyData;
}



__declspec(dllexport) bool responsePoint(const char* headerRequest, char** bodyResponse, uint64_t* responseSize, uint16_t* responseCode, char** adictionalHeader)
{
	request(headerRequest);
	return true;
}

__declspec(dllexport) bool requestPoint(char** headerRequest)
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
	options[0].optionString = "-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=5005";
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

__declspec(dllexport) const struct HTTP_plugin_metadata* getManifest()
{
	manifest.requestPoint = requestPoint;
	manifest.responsePoint = responsePoint;
	manifest.entryPoint = entryPoint;
	manifest.shutdownPoint = shutdownPoint;
	return &manifest;
}
