#include "HDS_UDP_Connection_Sender.h"
#include "../common/HDS_Thread.h"
#include "../common/atomic_t.h"
#include "../common/HDS_Blocking_Queue.h"

namespace HDS_UDP
{
	namespace Sender
	{
		static HDS_Thread_Data * _sender_thread_data_array=0;
		static char*                    _sender_buf=0;
		static int                         _sender_thread_count=0;
		static atomic_t<long>     _hexit=0;
		static HDS_Blocking_Queue<int>   _connection_sender_queue;

		static THREAD_RETURN_VALUE HDS_THREAD_API _sender_routine(void * parm)
		{
			int offset=0;
			while(!_hexit.value())
			{
				if(_connection_sender_queue.dequeue(offset,-1) == 0 )
				{
					Connection_Writer::connection_do_sender(offset,(char*)parm);
				}
			}
			return 0;
		}

		int init_sender(int thread_num)
		{
			_sender_thread_count = thread_num;
			if(_sender_thread_count == 0)
				_sender_thread_count = get_cpu_cores_count();

			_sender_thread_data_array = (HDS_Thread_Data*) malloc(_sender_thread_count*sizeof(HDS_Thread_Data));
			_sender_buf = (char*)malloc(_sender_thread_count*MAX_UDP_PACKET_SIZE);
			for(int i=0;i<_sender_thread_count;i++)
				HDS_Thread_Create(_sender_routine,&_sender_buf[i*MAX_UDP_PACKET_SIZE],&_sender_thread_data_array[i]);

			return 0;
		}

		int fini_sender()
		{
			_hexit = true;
			_connection_sender_queue.wait_up();
			for(int i=0;i<_sender_thread_count;i++)
				HDS_Thread_Join(&_sender_thread_data_array[i]);
			free(_sender_thread_data_array);
			free(_sender_buf);
			return 0;
		}

		int add_to_send_queue(const int offset)
		{
			_connection_sender_queue.enqueue(offset);
			return 0;
		}
	}
}