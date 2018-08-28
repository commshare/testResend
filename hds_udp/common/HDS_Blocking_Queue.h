#ifndef _HDS_BLOCKING_QUEUE_H_
#define _HDS_BLOCKING_QUEUE_H_
#include "s_list.h"
#include "atomic_t.h"
#include "AutoLock.h"
#include "HDS_Event.h"
#include <stdio.h>
//Ê¹ÓÃË«»º³å
template<typename T>
class HDS_Blocking_Queue
{
private:
	s_list<T>  _queue[2];
	atomic_t<long> _hexit;
	int _read_queue_cursor;
	int _write_queue_cursor;
	atomic_t<long> _reader_blocked;

#ifdef WIN32
	HANDLE                  _fillevent;
	CRITICAL_SECTION  _lock;
	CRITICAL_SECTION  _read_lock;
#else
	pthread_cond_t        _fillevent;
	pthread_mutex_t      _lock;
	pthread_mutex_t      _read_lock;
#endif
	
public:
	HDS_Blocking_Queue()
	{
		_read_queue_cursor =1;
		_write_queue_cursor=0;
		_reader_blocked = false;
#ifdef WIN32
		this->_fillevent = CreateEvent(0, TRUE, FALSE, 0);
		InitializeCriticalSection(&this->_lock);
		InitializeCriticalSection(&this->_read_lock);
#else
		pthread_mutex_init(&this->_lock,0);
		pthread_mutex_init(&this->_read_lock,0);
		pthread_cond_init(&this->_fillevent,0);
#endif
	}

	~HDS_Blocking_Queue()
	{
#ifdef WIN32
		CloseHandle(this->_fillevent);
		DeleteCriticalSection(&this->_lock);
#else
		pthread_mutex_destroy(&this->_lock);
		pthread_cond_destroy(&this->_fillevent);
#endif
	}


	inline void enqueue(const T& data)
	{
#ifdef WIN32
		EnterCriticalSection(&this->_lock);
#else
		pthread_mutex_lock(&this->_lock);
#endif
		this->_queue[_write_queue_cursor].push_back(data);
		if(_reader_blocked.CompareExchange(false,true) == (long)true)
		{
			wait_up();
		}
#ifdef WIN32
		LeaveCriticalSection(&this->_lock);
#else
		pthread_mutex_unlock(&this->_lock);
#endif
	}

	int dequeue(T& data , const long wait_time)
	{
#ifdef WIN32
		EnterCriticalSection(&this->_read_lock);
		if(this->_queue[_read_queue_cursor].dequeue_head(data) == 0)
		{
			LeaveCriticalSection(&this->_read_lock);
			return 0;
		}
		else
		{
			if(_reader_blocked.value() == false)
			{
				EnterCriticalSection(&this->_lock);
				if(this->_queue[_write_queue_cursor].size()>0)
				{
					this->_write_queue_cursor = !this->_write_queue_cursor;
					this->_read_queue_cursor = !this->_read_queue_cursor;
					LeaveCriticalSection(&this->_lock);
					this->_queue[_read_queue_cursor].dequeue_head(data);
					LeaveCriticalSection(&this->_read_lock);
					return 0;
				}
				ResetEvent(this->_fillevent);
				_reader_blocked = true;
				LeaveCriticalSection(&this->_lock);
				LeaveCriticalSection(&this->_read_lock);
			}
			else
			{
				ResetEvent(this->_fillevent);
				LeaveCriticalSection(&this->_read_lock);
			}
			
			if(WaitForSingleObject(this->_fillevent,wait_time)== WAIT_TIMEOUT)
				return -1;
			else
			{
				EnterCriticalSection(&this->_read_lock);
				int res = this->_queue[_read_queue_cursor].dequeue_head(data);
				LeaveCriticalSection(&this->_read_lock);
				if(res== -1)
					int x=0;
				return res;
			}
		}
#else
		pthread_mutex_lock(&this->_read_lock);
		if(this->_queue[_read_queue_cursor].size() >0 )
		{
			this->_queue[_read_queue_cursor].dequeue_head(data);
			pthread_mutex_unlock(&this->_read_lock);
			return 0;
		}
		else
		{
			if(this->_reader_blocked.value() == false)
			{
				pthread_mutex_lock(&this->_lock);
				if(this->_queue[_write_queue_cursor].size()>0)
				{
					this->_write_queue_cursor = !this->_write_queue_cursor;
					this->_read_queue_cursor = !this->_read_queue_cursor;
					pthread_mutex_unlock(&this->_lock);
					this->_queue[_read_queue_cursor].dequeue_head(data);
					pthread_mutex_unlock(&this->_read_lock);
					return 0;
				}
				this->_reader_blocked = true;
				pthread_mutex_unlock(&this->_lock);
			}

			int status;
			if(wait_time ==-1)
				status = pthread_cond_wait(&this->_fillevent, &this->_read_lock);
			else
			{
				struct timespec timeoutspc;
				set_time(&timeoutspc,wait_time);
				status = pthread_cond_timedwait(&this->_fillevent, &this->_read_lock, &timeoutspc);
			}

			if (status)
			{
				pthread_mutex_unlock(&this->_read_lock);
				return -1;
			}

			status = this->_queue[_read_queue_cursor].dequeue_head(data);
			pthread_mutex_unlock(&this->_read_lock);
			return status;
		}
#endif
	}

	inline void wait_up()
	{
#ifdef WIN32
		SetEvent(this->_fillevent);
#else
		pthread_cond_broadcast(&this->_fillevent);
#endif
	}

private:
};


#endif