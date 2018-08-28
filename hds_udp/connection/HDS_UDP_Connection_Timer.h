#ifndef _HDS_UDP_CONNECTION_TIMER_H_
#define _HDS_UDP_CONNECTION_TIMER_H_

#include "HDS_UDP_Connection.h"

namespace HDS_UDP
{
	namespace Timer
	{
		int init_timer_queue(int thread_num);

		int add_timer(long dwDueTime, void * parm);

		int fini_timer_queue();
	}
}

#endif