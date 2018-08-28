#ifndef _AUTO_LOCK_H_ 
#define _AUTO_LOCK_H_
#include "HDS_Mutex.h"

class AutoLock
{
private:
	HDS_Mutex & m_lock;
public:
	AutoLock(HDS_Mutex& lock):m_lock(lock)
	{
		HDS_Mutex_Lock(&lock);
	}

	~AutoLock()
	{
		HDS_Mutex_Unlock(&m_lock);
	}
};
#endif