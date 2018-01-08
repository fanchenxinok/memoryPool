#ifndef __MY_AUTO_LOCK__
#define __MY_AUTO_LOCK__

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* ���߳�����ͬһ���߳̾�����סҲ���Է����ٽ���Դ */
class myThreadLock
{
public:
	myThreadLock();
	~myThreadLock();
	void Lock();
	void unLock();

private:
#ifdef WIN32
    CRITICAL_SECTION win_critical_section;
#else
    pthread_mutex_t linux_mutex;
#endif
};


/* �Զ������� */

class AutoMutex
{
public:
	AutoMutex(myThreadLock *lock);
	~AutoMutex();
private:
	myThreadLock* pThreadLock;
};

#endif

