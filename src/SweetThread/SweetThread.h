#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <windows.h>
#define SWEETTHREAD_INIFINIT INFINITE
#define SWEETTHREAD_THREAD_RUNNING WAIT_TIMEOUT
#define SWEETTHREAD_THREAD_FINISHED WAIT_OBJECT_0
#define SWEETTHREAD_RETURN_VALUE(a) \
    return a
#define SWEETTHREAD_RETURN uint32_t
#endif

struct threadIdentifyer
{
    HANDLE address;
    DWORD id;
};

struct threadIdentifyer sweetThread_CreateThread(int32_t (*funcion)(void *functionParamets), void *argument, bool startNow);

struct threadIdentifyer sweetThread_CreateThread(int32_t (*funcion)(void *functionParamets), void *argument, bool startNow);

int32_t sweetThread_Join(struct threadIdentifyer thread, uint32_t milliseconds);

bool sweetThread_GetExitCode(struct threadIdentifyer thread, int32_t *exitCode);

void sweetThread_Resume(struct threadIdentifyer thread);

void sweetThread_Suspend(struct threadIdentifyer thread);

bool sweetThread_IsRunning(struct threadIdentifyer thread);

void sweetThread_Sleep(uint32_t milliseconds);
