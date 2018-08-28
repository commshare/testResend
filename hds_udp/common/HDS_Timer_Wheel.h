#ifndef _HDS_TIMER_WHEEL_H_
#define _HDS_TIMER_WHEEL_H_
#include "atomic_t.h"
#include "HDS_Thread.h"
#include "HDS_Unistd.h"
#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#include "HDS_Mutex.h"
#include "HDS_Event.h"


// -*- C++ -*-
//=============================================================================
/**
*  @file    Timer_Wheel_T.h
*
*  @jsunp
*/
//=============================================================================

typedef THREAD_RETURN_VALUE  (HDS_THREAD_API *HDS_timer_call_back)(void* lpParam);

#ifdef WIN32
template class HDS_UDP_EXPORT atomic_t<long>;
#endif

struct HDS_UDP_EXPORT HDS_Timer_Node
{
	HDS_timer_call_back _time_out_function;
	clock_t _expers_time;
	void * _parm;
	HDS_Timer_Node * next;
};

struct HDS_UDP_EXPORT HDS_Timer_Wheel_Bucket
{
	HDS_Timer_Node* _root;

	HDS_Timer_Wheel_Bucket():_root(0)
	{}

	inline void add_to_tail(HDS_Timer_Node* node)
	{
		if(node == 0)
			return;
		node->next=NULL;


		HDS_Timer_Node* tmp = this->_root;
		this->_root = node;
		this->_root->next = tmp;
	}

	inline int dequeue_head(HDS_Timer_Node* &node)
	{
		if(this->_root == NULL)
			return -1;

		node = this->_root;
		this->_root = node->next;

		node->next = NULL;
		return 0;
	}
};

struct HDS_UDP_EXPORT HDS_Timer_Wheel_Slot
{
	HDS_Timer_Node* _root;


	HDS_Timer_Wheel_Slot():_root(0)
	{}

	inline void add_to_tail(HDS_Timer_Node* node)
	{
		if(node == 0)
			return;
		node->next=NULL;

		HDS_Timer_Node* tmp = this->_root;
		this->_root = node;
		this->_root->next = tmp;
	}

	inline int dequeue_head(HDS_Timer_Node* &node)
	{
		if(this->_root == NULL)
			return -1;
		

		node = this->_root;
		this->_root = node->next;

		node->next = NULL;
		return 0;
	}
};

class HDS_UDP_EXPORT HDS_timer_wheel
{
public:
	HDS_timer_wheel(int thread_num);

	~HDS_timer_wheel();

	int add_timer(long DueTime,HDS_timer_call_back cal, void * parm);

	void exit();

private:
	int expire(HDS_Timer_Node* &node,long & next_exprie_time);

	void do_expire(HDS_Event* _event);

	int get_node_from_nearly_array(long cur_time,HDS_Timer_Node* &node,long & next_exprie_time);

	struct HDS_Timer_Wheel_Parm
	{
		HDS_timer_wheel * wl;
		HDS_Event* _event;
	};
	//时间相关
	struct timeval _start_tv;

	HDS_Timer_Wheel_Bucket _nearly_array[65536];
	HDS_Timer_Wheel_Slot _slot_array[65536];
	int _current_nearly_array_cursor;
	int _current_nearly_array_timer_count;
	long _total_timer_count;
	int _slot_cursor;

	//线程相关的数据结构
	static THREAD_RETURN_VALUE HDS_THREAD_API do_time_out(void* lpParam);
	HDS_Mutex _lock;
	HDS_Event *_event_array;
	HDS_Thread_Data * _timer_thread_data_array;
	HDS_Timer_Wheel_Parm* _timer_thread_parm_array;
	int _thread_count;
	int _wait_up_thread_cursor;
	atomic_t<long> _hexit;
};
#endif