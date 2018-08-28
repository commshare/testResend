#include "HDS_UDP_Connection_Recv_Wait_ACK_Queue.h"
#include "../common/HDS_Mutex.h"
#include "../common/HP_Map.h"

namespace HDS_UDP
{
	namespace Recv_Wait_Ack_Queue
	{
		HDS_Mutex _mutex;
		HP_map<Connection_ID,int>  _offset_map;

		int init()
		{
			HDS_Mutex_Init(&_mutex);
			return 0;
		}

		int fini()
		{
			HDS_Mutex_Delete(&_mutex);
			return 0;
		}

		int find(Connection_ID* con, int *offset)
		{
			HDS_Mutex_Lock(&_mutex);
			int res =_offset_map.find(*con,*offset);
			HDS_Mutex_Unlock(&_mutex);
			return res;
		}

		int insert(Connection_ID* con ,int offset)
		{
			HDS_Mutex_Lock(&_mutex);
			int res =_offset_map.insert(*con,offset);
			HDS_Mutex_Unlock(&_mutex);
			return res;
		}

		int erase(Connection_ID* con)
		{
			HDS_Mutex_Lock(&_mutex);
			int res =_offset_map.erase(*con);
			HDS_Mutex_Unlock(&_mutex);
			return res;
		}
	};
}