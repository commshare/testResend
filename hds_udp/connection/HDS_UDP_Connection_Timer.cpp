#include "HDS_UDP_Connection_Timer.h"
#include "../common/HDS_Timer_Wheel.h"


namespace HDS_UDP
{
	namespace Timer
	{

		static HDS_timer_wheel* _timer_wl=NULL;
		static int                       _thread_count=0;

		static THREAD_RETURN_VALUE  HDS_THREAD_API connection_timer_call_back(void* lpParam)
		{
			connection::connection_do_time_out((_timer_parm_*)lpParam);
			return 0;
		}

		int init_timer_queue(int thread_num)
		{
			_thread_count = thread_num;
			if(_thread_count == 0)
				_thread_count = get_cpu_cores_count();

			_timer_wl = new HDS_timer_wheel(_thread_count);
			return 0;
		}

		int add_timer(long dwDueTime, void * parm)
		{
			_timer_wl->add_timer(dwDueTime,connection_timer_call_back,parm);
			return 0;
		}

		int fini_timer_queue()
		{
			_timer_wl->exit();
			free(_timer_wl);
			return 0;
		}
	}
}
