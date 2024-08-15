#include "SweetThread.h"

#ifdef SWEEETTHREADWINDOWS
struct SweetThread_identifyer SweetThread_createThread(int32_t (*function)(void *functionParamets), void *argument, bool startNow)
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
#elif defined(SWEEETTHREADLINUX)

struct SweetThread_identifyer SweetThread_createThread(SWEETTHREAD_RETURN (*function)(void *functionParamets), void *argument, bool startNow)
{
    struct SweetThread_identifyer newThread;
    pthread_create((pthread_t *)(&newThread.address), NULL, function, (void *)argument);
    return newThread;
}

int32_t SweetThread_join(struct SweetThread_identifyer thread, uint32_t milliseconds)
{
    pthread_join(*((pthread_t *)(thread.address)), NULL);
    return 0;
}

bool SweetThread_getExitCode(struct SweetThread_identifyer thread, int32_t *exitCode)
{
    // pthread_join() não retorna o código de saída diretamente.
    // O valor pode ser obtido pela função que a thread retornou.
    return false; // Implementar de acordo com sua necessidade
}

void SweetThread_resume(struct SweetThread_identifyer thread)
{
    // Não há suporte direto para retomar threads em Linux
}

void SweetThread_suspend(struct SweetThread_identifyer thread)
{
    // Não há suporte direto para suspender threads em Linux
}

bool SweetThread_isRunning(struct SweetThread_identifyer thread)
{
    return SweetThread_join(thread, 1) == -1;
}

void SweetThread_sleep(uint32_t milliseconds)
{
    struct timespec time;
    time.tv_sec = milliseconds / 1000;
    time.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&time, NULL);
}
#endif