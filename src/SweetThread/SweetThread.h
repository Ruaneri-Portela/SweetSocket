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
#define SWEEETTHREADWINDOWS
struct SweetThread_identifyer
{
    HANDLE address;
    DWORD id;
};

#elif defined(__linux__) || defined(__unix__)
#include <pthread.h>
#define EXPORT
#define SWEEETTHREADLINUX
#define SWEETTHREAD_INIFINIT UINT32_MAX
#define SWEETTHREAD_RETURN void *
#define SWEETTHREAD_RETURN_VALUE(a) \
    pthread_exit((void *)a)

struct SweetThread_identifyer
{
    void *address;
    void *id;
};
#endif

#include <stdint.h>
#include <stdbool.h>

EXPORT struct SweetThread_identifyer SweetThread_createThread(SWEETTHREAD_RETURN (*function)(void *functionParamets), void *argument, bool startNow);

EXPORT int32_t SweetThread_join(struct SweetThread_identifyer thread, uint32_t milliseconds);

EXPORT bool SweetThread_getExitCode(struct SweetThread_identifyer thread, int32_t *exitCode);

EXPORT void SweetThread_resume(struct SweetThread_identifyer thread);

EXPORT void SweetThread_suspend(struct SweetThread_identifyer thread);

EXPORT bool SweetThread_isRunning(struct SweetThread_identifyer thread);

EXPORT void SweetThread_sleep(uint32_t milliseconds);
