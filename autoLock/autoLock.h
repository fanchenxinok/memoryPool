#ifndef __MY_AUTO_LOCK__
#define __MY_AUTO_LOCK__

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* 多线程锁，同一个线程尽管锁住也可以访问临界资源 */
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


/* 自动解锁类 */

class AutoMutex
{
public:
	AutoMutex(myThreadLock *lock);
	~AutoMutex();
private:
	myThreadLock* pThreadLock;
};

#endif

