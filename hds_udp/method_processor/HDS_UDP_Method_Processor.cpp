#include "HDS_UDP_Method_Processor.h"
#include "../common/HDS_Thread.h"
#include "../common/HDS_Unistd.h"
#include "../common/HP_Map.h"
#include "../common/HDS_Blocking_Queue.h"
#include "../common/HDS_Error.h"
#include "../common/HDS_ReadCDR.h"
#include "../common/HDS_Unistd.h"
#include "../buffer/HDS_Buffer_Pool.h"

#ifdef _SUPPORT_ACE_

#ifdef _DEBUG
#pragma comment(lib,"aced.lib")
#else
#pragma comment(lib,"ace.lib")
#endif

#endif

namespace HDS_UDP
{
	namespace HDS_UDP_Method_Processor
	{
		static HDS_Thread_Data * _worker_thread_data_array=0;
		static int _worker_thread_count=0;
		static HP_map<MsgType, PI_UDP_Processor>  _method_tree;
		static HDS_Blocking_Queue<HDS_UDP_Result_Process_Data>  _worker_data_queue;
		static atomic_t<long>   _hexit;

		static THREAD_RETURN_VALUE HDS_THREAD_API _method_processor(void * parm)
		{
			MsgType msgtype;
			HDS_UDP_Result_Process_Data process_data;
			PI_UDP_Processor processor;
			while(! _hexit.value())
			{
				if(_worker_data_queue.dequeue(process_data,-1) !=0)
					continue;

				HDS_ReaderCDR cdr(process_data.data);
				cdr.read_long(msgtype);
				if(_method_tree.find(msgtype,processor)==0)
				{
#ifdef _SUPPORT_ACE_
					ACE_Message_Block ace_block(process_data.data,process_data.data_length);
					ace_block.msg_type(process_data.buf_type);
					ace_block.wr_ptr(process_data.data_length);
					ACE_INET_Addr ace_addr((sockaddr_in*)&process_data.remote_addr,process_data.remote_addr_length);
					processor->process(&ace_block,ace_addr);
					process_data.buf_type = (HDS_UDP_Recv_Buf_Type)ace_block.msg_type();
#else
					processor(&process_data);
#endif
				}

				switch(process_data.buf_type)
				{
				case USER_SPACE_NO_DEL:
				case MALLOC_SPACE_NO_DEL:
					{
						break;
					}
				case POOL_SPACE:
					{
						Buffer_Pool::release_buffer(process_data.data,process_data.data_length);
						break;
					}
				case MALLOC_SPACE_DEL:
				case USER_SPACE_DEL:
					{
						free(process_data.data);
						break;
					}
				}
			}
			return 0;
		}

#ifdef _SUPPORT_ACE_
		int query_separate_recv_buf_AceBlock(ACE_Message_Block* block)
		{
			switch(block->msg_type())
			{
			case POOL_SPACE:
				return -1;
			case USER_SPACE_DEL:
				{
					block->msg_type(USER_SPACE_NO_DEL);
					return 0;
				}
			case MALLOC_SPACE_DEL:
				{
					block->msg_type( MALLOC_SPACE_NO_DEL);
					return 0;
				}
			default:
				return 0;
			}
		}

#else
		int query_separate_recv_buf(HDS_UDP_Result_Process_Data* process_data)
		{
			switch(process_data->buf_type)
			{
			case POOL_SPACE:
				return -1;
			case USER_SPACE_DEL:
				{
					process_data->buf_type = USER_SPACE_NO_DEL;
					return 0;
				}
			case MALLOC_SPACE_DEL:
				{
					process_data->buf_type = MALLOC_SPACE_NO_DEL;
					return 0;
				}
			default:
				return 0;
			}
		}
#endif


		int init_processor(const int& threads_count)
		{
			_worker_thread_count = threads_count;
			if(_worker_thread_count == 0)
				_worker_thread_count = get_cpu_cores_count();

			_worker_thread_data_array = (HDS_Thread_Data*)malloc(sizeof(HDS_Thread_Data)*_worker_thread_count);
			for(int i=0;i<_worker_thread_count;i++)
			{
				if(HDS_Thread_Create(_method_processor,NULL,&_worker_thread_data_array[i])==-1)
				{
					print_error("create method processor");
					free(_worker_thread_data_array);
					return -1;
				}
			}

			return 0;
		}

		int registe_method(MsgType msgType, const PI_UDP_Processor proc)
		{
			return _method_tree.insert(msgType,proc);
		}

		void push_msg(const HDS_UDP_Result_Process_Data& process_data)
		{
			_worker_data_queue.enqueue(process_data);
		}

		int fini_porcessor()
		{
			_hexit = true;
			_worker_data_queue.wait_up();
			for(int i=0;i<_worker_thread_count;i++)
				HDS_Thread_Join(&_worker_thread_data_array[i]);

			free(_worker_thread_data_array);
			return 0;
		}

	}
}