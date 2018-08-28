#ifndef _HDS_UDP_COMPLEX_CONNECTION_WRITER_H_
#define _HDS_UDP_COMPLEX_CONNECTION_WRITER_H_

#include "HDS_UDP_Connection.h"
#include "HDS_UDP_Connection_Ack_Info_Collector.h"

namespace HDS_UDP
{
	class HDS_UDP_Complex_Connection_Writer:public Connection_Writer
	{
	public:
		volatile unsigned int _current_window_cursor; 
		volatile unsigned int _old_current_window_cursor;
		atomic_t<unsigned long> _right_window_cursor;
		volatile unsigned long _window_site;
		volatile unsigned long _window_ssthresh;
		volatile unsigned long _default_window_size;
		volatile unsigned long _sack_seq;
#ifdef _USE_COMPLEX_FILE_WINDOW_INCREASE_FACTOR_
		volatile float    _ack_seq_window_increase_factor;
#endif
		volatile unsigned int    _sack_same_count;
		static HDS_UDP_Complex_Connection_Writer* malloc();
		void construct();
		static unsigned long get_time_out_count();
		static unsigned long get_conenction_count();
	};

	class HDS_UDP_Complex_Connection_Reader: public Connection_Reader
	{
	public:
		HDS_UDP_Connection_Ack_Info_Collector _ack_info_collector;
		static HDS_UDP_Complex_Connection_Reader* malloc();
		static unsigned long get_conenction_count();
		void construct();
	};
}


#endif