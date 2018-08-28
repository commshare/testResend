#ifndef  _HDS_MUTEX_H_ 
#define _HDS_MUTEX_H_

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>

typedef CRITICAL_SECTION HDS_Mutex;
#define HDS_Mutex_Init(ptr_lock)         do{::InitializeCriticalSection(ptr_lock);}while(0)
#define HDS_Mutex_Lock(ptr_lock)       do{::EnterCriticalSection(ptr_lock);}while(0)
#define HDS_Mutex_TryLock(ptr_lock)   do{::TryEnterCriticalSection(ptr_lock);}while(0)
#define HDS_Mutex_Unlock(ptr_lock)    do{::LeaveCriticalSection(ptr_lock);}while(0)
#define HDS_Mutex_Delete(ptr_lock)     do{::DeleteCriticalSection(ptr_lock);}while(0)

#else
//Linux�µ�pthread_mutex_t��Ĭ���Ƿǵݹ�ġ�
//������ʾ������PTHREAD_MUTEX_RECURSIVE���ԣ���pthread_mutex_t��Ϊ�ݹ���
#include <pthread.h>

typedef pthread_mutex_t HDS_Mutex;
#define HDS_Mutex_Init(ptr_lock)         do{::pthread_mutex_init(ptr_lock,NULL);}while(0)
#define HDS_Mutex_Lock(ptr_lock)       do{::pthread_mutex_lock(ptr_lock);}while(0)
#define HDS_Mutex_TryLock(ptr_lock)   do{::pthread_mutex_trylock(ptr_lock);}while(0)
#define HDS_Mutex_Unlock(ptr_lock)    do{::pthread_mutex_unlock(ptr_lock);}while(0)
#define HDS_Mutex_Delete(ptr_lock)     do{::pthread_mutex_destroy(ptr_lock);}while(0)

#endif

#endif