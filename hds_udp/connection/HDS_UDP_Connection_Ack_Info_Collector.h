#ifndef _HDS_UDP_CONNECTION_ACK_INFO_COLLECTOR_H_
#define _HDS_UDP_CONNECTION_ACK_INFO_COLLECTOR_H_

#include "../config/HDS_UDP_Kernel_Config.h"
#include "../common/atomic_t.h"
#include "HDS_UDP_Connection.h"

namespace HDS_UDP
{
	class HDS_UDP_Connection_Ack_Info_Collector
	{
	private:
		atomic_t<long> _bitmap[HDS_UDP_CONNECTION_RECV_BITMAP_SIZE/sizeof(long)];
		atomic_t<unsigned long> _last_ack_pos;
		atomic_t<unsigned long> _max_seq;
		atomic_t<long> _ack_time;
		atomic_t<long> _last_recv_time;

	private:
		Ack_Type _get_next_ack(unsigned int & seq);

	public:
		HDS_UDP_Connection_Ack_Info_Collector();

		int set(const unsigned int seq,const int recv_time,int* oldvalue);

		Ack_Type get_next_ack(unsigned int & seq, int & ack_time);

	};
}
#endif