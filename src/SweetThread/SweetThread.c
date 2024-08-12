#include "SweetThread.h"

struct SweetThread_identifyer SweetThread_createThread(int32_t(*function)(void *functionParamets), void *argument, bool startNow)
{
    struct SweetThread_identifyer newThread;
    DWORD initThreadParamets = startNow ? 0 : CREATE_SUSPENDED;
    newThread.address = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)function, argument, initThreadParamets, &newThread.id);
    return newThread;
}

int32_t SweetThread_join(struct SweetThread_identifyer thread, uint32_t milliseconds)
{
    return WaitForSingleObject(thread.address, milliseconds);
}

bool SweetThread_getExitCode(struct SweetThread_identifyer thread, int32_t *exitCode)
{
    return GetExitCodeThread(thread.address, (LPDWORD)exitCode);
}

void SweetThread_resume(struct SweetThread_identifyer thread)
{
    ResumeThread(thread.address);
}

void SweetThread_suspend(struct SweetThread_identifyer thread)
{
    SuspendThread(thread.address);
}

bool SweetThread_isRunning(struct SweetThread_identifyer thread)
{
    return SweetThread_join(thread, 1) == SWEETTHREAD_THREAD_RUNNING;
}

void SweetThread_sleep(uint32_t milliseconds)
{
    Sleep(milliseconds);
}