#ifndef _HDS_THREAD_H_
#define _HDS_THREAD_H_

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <pthread.h>
#endif

#ifdef WIN32
typedef DWORD THREAD_RETURN_VALUE ;
#define HDS_THREAD_API WINAPI
#else
typedef void* THREAD_RETURN_VALUE;
#define HDS_THREAD_API
#endif

typedef THREAD_RETURN_VALUE (HDS_THREAD_API *HDS_Thread_Routine)(void *);

struct HDS_Thread_Data
{
#ifdef WIN32
	HANDLE hThread;
	DWORD dwThreadId;
#else
	int hThread;
	pthread_t dwThreadId;
#endif
};

static inline int HDS_Thread_Create(HDS_Thread_Routine func,void* parm,HDS_Thread_Data* data)
{
#ifdef WIN32
	data->hThread = CreateThread(NULL,NULL,func,parm,0,&data->dwThreadId);
	if(data->hThread == NULL)
		return -1;
	return 0;
#else
	return pthread_create(&data->dwThreadId,NULL,func, parm);
#endif
}

static inline int HDS_Thread_Join(HDS_Thread_Data* data)
{
#ifdef WIN32
	WaitForSingleObject(data->hThread,INFINITE);
	return 0;
#else
	return pthread_join(data->dwThreadId, NULL);
#endif
}

#endif