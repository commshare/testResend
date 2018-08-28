#include "HDS_Timer_Wheel.h"
#include "AutoLock.h"
#include "assert.h"
#include "HDS_Error.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../buffer/HDS_Buffer_Pool.h"
#include "HDS_Unistd.h"

#ifndef WIN32
#include <stdint.h>
#endif


/*
定时器队列，此队列提供多线程的支持
*/
typedef HDS_UDP::HDS_Singleton_Pool<sizeof(HDS_Timer_Node)> HDS_Timer_Node_Pool;

THREAD_RETURN_VALUE HDS_THREAD_API  HDS_timer_wheel::do_time_out(void* lpParam)
{
	HDS_Timer_Wheel_Parm * parm = reinterpret_cast<HDS_Timer_Wheel_Parm*>(lpParam);
	parm->wl->do_expire(parm->_event);
	return 0;
}

#define WHEEL_SHIFT_MASK 0xffff

#ifndef __LP64__
#define time_after(a,b)\
	((long)(b) - (long)(a) < 0) 
#define time_before(a,b)    time_after(b,a) 
#define time_after_eq(a,b)    \
	((long)(a) - (long)(b) >= 0)
#define time_before_eq(a,b)    time_after_eq(b,a)
#else
#define WHEEL_INT_MASK    0xffffffff
static inline int  time_after(long a,long b)
{
	int ta = a&WHEEL_INT_MASK;
	int tb = b&WHEEL_INT_MASK;
	return tb-ta<0;
}

static inline int  time_before(long a,long b)
{
	return time_after(b,a);
}

static inline int  time_after_eq(long a,long b)
{
	int ta = a&WHEEL_INT_MASK;
	int tb = b&WHEEL_INT_MASK;
	return ta-tb>=0;
}

static inline int  time_before_eq(long a,long b)
{
	return time_after_eq(b,a);
}
#endif

HDS_timer_wheel::HDS_timer_wheel(int thread_num):
_slot_cursor(0),
_current_nearly_array_cursor(0),
_total_timer_count(0),
_wait_up_thread_cursor(0),
_current_nearly_array_timer_count(0),
_hexit(false),
_thread_count(thread_num)
{
	HDS_Mutex_Init(&this->_lock);
	memset(this->_nearly_array,0,sizeof(HDS_Timer_Wheel_Bucket)<<16);
	memset(this->_slot_array,0,sizeof(HDS_Timer_Wheel_Slot)<<16);
	_event_array = (HDS_Event*)malloc(sizeof(HDS_Event)*thread_num);
	_timer_thread_data_array = (HDS_Thread_Data*)malloc(sizeof(HDS_Thread_Data)*thread_num);
	_timer_thread_parm_array = (HDS_Timer_Wheel_Parm*)malloc(sizeof(HDS_Timer_Wheel_Parm)*thread_num);
	for(int i= 0; i< thread_num;i++)
	{
		_timer_thread_parm_array[i].wl = this;
		HDS_Event_Init(&_event_array[i],false);
		_timer_thread_parm_array[i]._event = &_event_array[i];

		if(HDS_Thread_Create(HDS_timer_wheel::do_time_out,&_timer_thread_parm_array[i],&_timer_thread_data_array[i])!=0)
			print_error("A worker thread was not able to be created. GetLastError returned");
	}

	gettimeofday(&this->_start_tv,NULL);
}


#ifdef WIN32
#define wl_clock(tv,time_wheel) \
	do{\
	tv=clock();\
	}while(0)
#else 
#define wl_clock(tv,time_wheel) \
	do{\
	timeval dif;\
	gettimeofday(&dif,NULL);\
	timeval_sub(dif,time_wheel->_start_tv);\
	utimevalue uv = dif.tv_sec*1000+dif.tv_usec/1000;\
	tv = static_cast<long>(uv & 0xffffffff);\
	}while(0)
#endif

int HDS_timer_wheel::add_timer(long DueTime,HDS_timer_call_back cal, void * parm)
{
	if(_hexit.value())
		return -1;

	if(DueTime>0x0fffffff)
		return -1;

	HDS_Timer_Node * node = reinterpret_cast<HDS_Timer_Node*>(HDS_Timer_Node_Pool::malloc());
	memset(node,0,sizeof(HDS_Timer_Node));

	AutoLock locker(this->_lock);
	if(node == NULL)
		return -1;

	this->_total_timer_count++;

	long wl_clock_time;
	wl_clock(wl_clock_time,this);

	long xtime =DueTime + wl_clock_time;
	node->_expers_time = xtime;
	node->_parm = parm;
	node->_time_out_function = cal;

	int slot_num = xtime>>16;
	if(slot_num == this->_slot_cursor&&this->_current_nearly_array_cursor<65536)
	{
		int bucket = xtime % 65536;
		if(this->_current_nearly_array_cursor>bucket)
			bucket = this->_current_nearly_array_cursor;

		_nearly_array[bucket].add_to_tail(node);
		this->_current_nearly_array_timer_count++;

		HDS_Event_Set(&_event_array[this->_wait_up_thread_cursor]);
		if(++this->_wait_up_thread_cursor>=this->_thread_count)
			this->_wait_up_thread_cursor=0;

		return 0;
	}
	else
		this->_slot_array[slot_num].add_to_tail(node);

	if(this->_total_timer_count== 1 )
	{
		HDS_Event_Set(&_event_array[this->_wait_up_thread_cursor]);
		if(++this->_wait_up_thread_cursor>=this->_thread_count)
			this->_wait_up_thread_cursor=0;
	}
	return 0;
}


//此函数用于从所在的array中找到一个node，如果没有任何可用的node，返回1
int HDS_timer_wheel::get_node_from_nearly_array(long cur_time,HDS_Timer_Node* &node,long & next_exprie_time)
{
	int index = (cur_time & WHEEL_SHIFT_MASK); 
	for(;this->_current_nearly_array_cursor<=index;this->_current_nearly_array_cursor++)
	{
		if(this->_nearly_array[this->_current_nearly_array_cursor]._root != NULL)
		{
				this->_total_timer_count--;
				this->_current_nearly_array_timer_count--;
				this->_nearly_array[this->_current_nearly_array_cursor].dequeue_head(node);
				return 0;
		}
	}
	for(;index < 65536; index++)
	{
		if(this->_nearly_array[index]._root != NULL )
			break;
	}
	next_exprie_time = index - (cur_time & WHEEL_SHIFT_MASK);
	return -1;
}
int HDS_timer_wheel::expire(HDS_Timer_Node*& retnode, long & next_exprie_time)
{
	AutoLock locker(this->_lock);
	if(this->_total_timer_count == 0)
	{
		next_exprie_time = INFINITE;
		return -1;
	}
	long  cur_time;
	wl_clock(cur_time,this);
	long  slot_base = this->_slot_cursor<<16;
	if(this->_current_nearly_array_timer_count!=0)
	{
		if(time_after(cur_time, (slot_base | WHEEL_SHIFT_MASK )))
			cur_time = slot_base | WHEEL_SHIFT_MASK;
		return get_node_from_nearly_array(cur_time,retnode,next_exprie_time);
	}

	if(time_before_eq(cur_time, (slot_base | WHEEL_SHIFT_MASK)))
	{
		next_exprie_time = WHEEL_SHIFT_MASK -  (cur_time & WHEEL_SHIFT_MASK)+1;
		return -1;
	}
		
	//需要转动轮子了
	memset(this->_nearly_array,0,sizeof(HDS_Timer_Wheel_Bucket)<<16);
	this->_current_nearly_array_cursor = 0;
	int next_slot_cursor = this->_slot_cursor;
	while(true)
	{
		next_slot_cursor++;
		if(next_slot_cursor == 65536)
			this->_slot_cursor=0;

		if(time_before_eq(next_slot_cursor<<16,cur_time))
		{
			//转动轮子
			this->_slot_cursor = next_slot_cursor;

			if(this->_slot_array[this->_slot_cursor]._root != NULL)
			{
				HDS_Timer_Node* xnode = NULL;
				while(this->_slot_array[this->_slot_cursor].dequeue_head(xnode)==0)
				{
					this->_nearly_array[xnode->_expers_time%65536].add_to_tail(xnode);
					this->_current_nearly_array_timer_count++;
				}
				if(time_before_eq((this->_slot_cursor+1)<<16,cur_time))
					cur_time = cur_time | WHEEL_SHIFT_MASK;
				return get_node_from_nearly_array(cur_time,retnode,next_exprie_time);
			}
		}
		else
		{
			this->_slot_cursor = next_slot_cursor-1;
			next_exprie_time = WHEEL_SHIFT_MASK -( cur_time & WHEEL_SHIFT_MASK)+1;
			return -1;
		}

	}
	next_exprie_time = INFINITE;
	return -1;
}

void HDS_timer_wheel::do_expire(HDS_Event* _event)
{
	HDS_Timer_Node* node;
	long next_expire_time=0;
	while(true)
	{
		if(this->_hexit.value())
			break;

		if(this->expire(node,next_expire_time) == 0)
		{
			node->_time_out_function(node->_parm);
			HDS_Timer_Node_Pool::free(node);
		}
		else
		{
			HDS_Event_Reset(_event);
			HDS_Event_Wait(_event,next_expire_time);
		}
	}
}

void HDS_timer_wheel::exit()
{
	this->_hexit = true;
	for(int i=0;i<this->_thread_count;i++)
		HDS_Event_Set(&this->_event_array[i]);

	//thread exit
	for(int i=0;i<this->_thread_count;i++)
	{
		HDS_Thread_Join(&this->_timer_thread_data_array[i]);
	}
}

HDS_timer_wheel::~HDS_timer_wheel()
{
	for(int i=0;i<this->_thread_count;i++)
		HDS_Event_Delete(&this->_event_array[i]);

	HDS_Mutex_Lock(&this->_lock);
	HDS_Mutex_Unlock(&this->_lock);
	HDS_Mutex_Delete(&this->_lock);

	free(this->_event_array);
	free(this->_timer_thread_data_array);
	free(this->_timer_thread_parm_array);
}