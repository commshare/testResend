#ifndef _HDS_UDP_CONNECTION_RECV_WAIT_ACK_QUEUE_H_
#define _HDS_UDP_CONNECTION_RECV_WAIT_ACK_QUEUE_H_
#include "HDS_UDP_Connection.h"

namespace HDS_UDP
{
	namespace Recv_Wait_Ack_Queue
	{
		int init();

		int fini();

		int find(Connection_ID* con_ID, int *offset);

		int insert(Connection_ID* con_ID ,int offset);

		int erase(Connection_ID* con_ID);
	};
}


#endif