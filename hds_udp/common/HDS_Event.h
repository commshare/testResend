#ifndef     _HDS_EVENT_H_
#define  _HDS_EVENT_H_

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
typedef HANDLE HDS_Event;


//************************************************************************
//*@jsunp
//*
//*需要注意在linux平台上有假唤醒的情况。当收到到SIGSTP信号时，会出现假唤醒
//************************************************************************


//default  value is not set;
//@0 sucessful
//@-1 failed
inline int HDS_Event_Init(HDS_Event* ev,int bInitalValue)
{
	 *ev = CreateEvent(0, TRUE, bInitalValue, 0);
	 return *ev>0 ? 0 : -1;
}


inline int HDS_Event_Wait(HDS_Event* ev,const long timeout=-1)
{
	return WaitForSingleObject(*ev,timeout);
}

//If the function succeeds, the return value indicates the event that caused the function to return. 
//It can be one of the following values. 
//(Note that WAIT_OBJECT_0 is defined as 0 and WAIT_ABANDONED_0 is defined as 0x00000080L.)
//inline int HDS_Event_Wait_Multiobject(const long count, const HDS_Event** ev,const long timeout)
//{
//	return WaitForMultipleObjects(count,*ev,1,timeout);
//}


//If the function succeeds, the return value is nonzero.
//If the function fails, the return value is zero. To get extended error information, call GetLastError.
inline int HDS_Event_Set(HDS_Event* ev)
{
	return SetEvent(*ev);
}


//If the function succeeds, the return value is nonzero.
//If the function fails, the return value is zero. To get extended error information, call GetLastError.
inline int HDS_Event_Reset(HDS_Event* ev)
{
	return ResetEvent(*ev);
}


//If the function succeeds, the return value is nonzero.
//If the function fails, the return value is zero. To get extended error information, call GetLastError.
//If the application is running under a debugger, 
//the function will throw an exception if it receives either a handle value that is not valid or a pseudo-handle value.
//This can happen if you close a handle twice, 
//or if you call CloseHandle on a handle returned by the FindFirstFile function instead of calling the FindClose function.
inline int HDS_Event_Delete(HDS_Event *ev)
{
	return CloseHandle(*ev);
}

#else

#include <pthread.h>
#include <stdbool.h>
#include "HDS_Unistd.h"
typedef struct
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool signal;
}HDS_Event;

//default  value is not set;
inline int HDS_Event_Init(HDS_Event* ev,int bInitalValue)
{
	// pthread_mutexattr_t attr;
	//pthread_mutexattr_init(&attr);
	//pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_NORMAL);
	pthread_mutex_init(&ev->mutex, 0);
	pthread_cond_init(&ev->cond, 0);
	ev->signal = (bInitalValue==0)?0:1;
	return 0;
}

inline void  set_time(struct timespec * spc, const long timeout)
{
	struct timeval now;
	gettimeofday(&now,NULL);
	spc->tv_sec = now.tv_sec + timeout / 1000;
	spc->tv_nsec = now.tv_usec * 1000 + (timeout%1000)*1000000;

#define BILLION 1000000000
	if (spc->tv_nsec >= BILLION)
	{
		spc->tv_sec += spc->tv_nsec / BILLION;
		spc->tv_nsec = spc->tv_nsec % BILLION;
	}
}

inline int HDS_Event_Wait(HDS_Event* ev,const long timeout=-1)
{
	int status;
	pthread_mutex_lock(&ev->mutex);
	if(ev->signal)
	{
		pthread_mutex_unlock(&ev->mutex);
		return 0;
	}

	if(timeout==-1)
		status = pthread_cond_wait(&ev->cond, &ev->mutex);
	else
	{
		struct timespec timeoutspc;
		set_time(&timeoutspc,timeout);
		status = pthread_cond_timedwait(&ev->cond, &ev->mutex, &timeoutspc);
	}

	if (status)
	{
		pthread_mutex_unlock(&ev->mutex);
		return -1;
	}

	pthread_mutex_unlock(&ev->mutex);
	return 0;
}


inline int HDS_Event_Set(HDS_Event* ev)
{
	pthread_mutex_lock(&ev->mutex);
	if(ev->signal)
	{
		pthread_mutex_unlock(&ev->mutex);
		return 0;
	}

	ev->signal = 1;
	pthread_cond_broadcast(&ev->cond);
	pthread_mutex_unlock(&ev->mutex);
	return 0;
}

inline int HDS_Event_Reset(HDS_Event* ev)
{
	pthread_mutex_lock(&ev->mutex);
	if(!ev->signal)
	{
		pthread_mutex_unlock(&ev->mutex);
		return 0;
	}
	ev->signal = 0;
	pthread_mutex_unlock(&ev->mutex);
	return 0;
}

inline int HDS_Event_Delete(HDS_Event *ev)
{
	pthread_mutex_destroy(&ev->mutex);
	pthread_cond_destroy(&ev->cond);
	return 0;
}

#endif


#endif