#ifndef   _HDS_UDP_CONNECTION_SENDER_H_
#define  _HDS_UDP_CONNECTION_SENDER_H_
#include "HDS_UDP_Connection.h"


namespace HDS_UDP
{
	namespace Sender
	{
		int init_sender(int thread_num);

		int fini_sender();

		int add_to_send_queue(const  int connection_array_offset);
	}
}



#endif