#include "SweetThread.h"

struct threadIdentifyer sweetThread_CreateThread(int32_t (*funcion)(void *functionParamets), void *argument, bool startNow)
{
    struct threadIdentifyer newThread;
    DWORD initThreadParamets = startNow ? 0 : CREATE_SUSPENDED;
    newThread.address = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)funcion, argument, initThreadParamets, &newThread.id);
    return newThread;
};

int32_t sweetThread_Join(struct threadIdentifyer thread, uint32_t milliseconds)
{
    return WaitForSingleObject(thread.address, milliseconds);
};

bool sweetThread_GetExitCode(struct threadIdentifyer thread, int32_t *exitCode)
{
    return GetExitCodeThread(thread.address, exitCode);
};

void sweetThread_Resume(struct threadIdentifyer thread)
{
    ResumeThread(thread.address);
};

void sweetThread_Suspend(struct threadIdentifyer thread)
{
    SuspendThread(thread.address);
};

bool sweetThread_IsRunning(struct threadIdentifyer thread)
{
    return sweetThread_Join(thread, 1) == SWEETTHREAD_THREAD_RUNNING;
};

void sweetThread_Sleep(uint32_t milliseconds)
{
    Sleep(milliseconds);
};