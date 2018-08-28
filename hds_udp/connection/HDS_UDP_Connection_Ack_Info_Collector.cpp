#include "HDS_UDP_Connection_Ack_Info_Collector.h"
#include "../common/Atomic_Bitmap.h"

namespace HDS_UDP
{
#define MAX_BIT_NUM            (HDS_UDP_CONNECTION_RECV_BITMAP_SIZE*8)
#define MAX_LONG_NUM        (HDS_UDP_CONNECTION_RECV_BITMAP_SIZE/sizeof(long))


	HDS_UDP_Connection_Ack_Info_Collector::HDS_UDP_Connection_Ack_Info_Collector():_last_ack_pos(0),
		_max_seq(0),
		_ack_time(0),
		_last_recv_time(0)
	{
		memset(&_bitmap,0,HDS_UDP_CONNECTION_RECV_BITMAP_SIZE);
	}

	int HDS_UDP_Connection_Ack_Info_Collector::set(const unsigned int seq,const int recv_time,int* oldvalue)
	{
		unsigned int inpos = seq / HDS_UDP_PACKET_MAX_DATA_LENGTH;
		if(seq > _max_seq.value())
			_max_seq = seq;

		if( inpos < _last_ack_pos.value())
			return -1;

		if(inpos >= (_last_ack_pos.value()+ MAX_BIT_NUM))
		{
			//need to compize
			unsigned int tpseq;
			_get_next_ack(tpseq);
			if(inpos >= (_last_ack_pos.value()+ MAX_BIT_NUM))
			{
				return -1; //after compize,still not work, so we return.
			}
		}

		_ack_time.CompareExchange((long)recv_time,0);
		_last_recv_time = (long)recv_time; 

		inpos = inpos %  MAX_BIT_NUM;
		*oldvalue = _bitmap[inpos/LONG_HAS_BIT].AtomicBitTestAndSet(inpos%LONG_HAS_BIT);
		return 0;
	}

	Ack_Type HDS_UDP_Connection_Ack_Info_Collector::get_next_ack(unsigned int & seq, int & ack_time)
	{
		ack_time = (int)this->_ack_time.AtomicExchange(0);
		if(ack_time== 0)
		{
			ack_time = (int)this->_last_recv_time.value();
		}
		return _get_next_ack(seq);
	}

	Ack_Type HDS_UDP_Connection_Ack_Info_Collector::_get_next_ack(unsigned int & seq)
	{
		int start_pos = 0;
		int start_ack_pos=0;
		while(true)
		{
			start_ack_pos = _last_ack_pos.value();
			start_pos = start_ack_pos% MAX_BIT_NUM;
			if(this->_bitmap[start_pos/LONG_HAS_BIT].AtomicBitTestAndReset(start_pos%LONG_HAS_BIT)==0)
				goto find_done;

			this->_last_ack_pos.CompareExchange(start_ack_pos+1,start_ack_pos);
		}

find_done:
		seq =(_last_ack_pos.value())*HDS_UDP_PACKET_MAX_DATA_LENGTH;

		if(_max_seq != (seq - HDS_UDP_PACKET_MAX_DATA_LENGTH))
			return SACK;
		else 
			return ACK;
	}
}