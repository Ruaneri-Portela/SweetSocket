#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <windows.h>
#define EXPORT __declspec(dllexport)
#define SWEETTHREAD_INIFINIT INFINITE
#define SWEETTHREAD_THREAD_RUNNING WAIT_TIMEOUT
#define SWEETTHREAD_THREAD_FINISHED WAIT_OBJECT_0
#define SWEETTHREAD_RETURN_VALUE(a) \
    return a
#define SWEETTHREAD_RETURN int32_t
#endif

#include <stdint.h>
#include <stdbool.h>

struct threadIdentifyer
{
    HANDLE address;
    DWORD id;
};

EXPORT struct threadIdentifyer sweetThread_CreateThread(int32_t (*function)(void *functionParamets), void *argument, bool startNow);

EXPORT int32_t sweetThread_Join(struct threadIdentifyer thread, uint32_t milliseconds);

EXPORT bool sweetThread_GetExitCode(struct threadIdentifyer thread, int32_t *exitCode);

EXPORT void sweetThread_Resume(struct threadIdentifyer thread);

EXPORT void sweetThread_Suspend(struct threadIdentifyer thread);

EXPORT bool sweetThread_IsRunning(struct threadIdentifyer thread);

EXPORT void sweetThread_Sleep(uint32_t milliseconds);
