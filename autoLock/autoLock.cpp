#include "autoLock.h"

/* ���߳�����ͬһ���߳̾�����סҲ���Է����ٽ���Դ */
void myThreadLock::Lock()
{
#ifdef WIN32
	EnterCriticalSection(&win_critical_section);
#else
	pthread_mutex_lock(&linux_mutex);
#endif
}

void myThreadLock::unLock()
{
#ifdef WIN32
	LeaveCriticalSection(&win_critical_section);
#else
	pthread_mutex_unlock(&linux_mutex);
#endif
}

myThreadLock::myThreadLock()
{
#ifdef WIN32
	InitializeCriticalSection(&win_critical_section);
#else
	pthread_mutex_init(&linux_mutex, NULL);
#endif
}

myThreadLock::~myThreadLock()
{
#ifdef WIN32
	DeleteCriticalSection(&win_critical_section);
#else
	pthread_mutex_unlock(&linux_mutex);
#endif
}


/* �Զ������� */
AutoMutex::AutoMutex(myThreadLock *lock)
{
	pThreadLock = lock;
	if(pThreadLock != NULL)
		pThreadLock->Lock();
}

AutoMutex::~AutoMutex()
{
	if(pThreadLock != NULL)
		pThreadLock->unLock();
}


