#ifndef _HDS_UDP_SIMPLE_CONNECTION_WRITER_H_
#define _HDS_UDP_SIMPLE_CONNECTION_WRITER_H_

#include "HDS_UDP_Connection.h"

namespace HDS_UDP
{
	class HDS_UDP_Simple_Connection_Writer:public Connection_Writer
	{
	public:
		static unsigned long get_time_out_count();
		static unsigned long get_conenction_count();
		static HDS_UDP_Simple_Connection_Writer* malloc();
		void construct();
	};


	class HDS_UDP_Simple_Connection_Reader:public Connection_Reader
	{
	public:
		atomic_t<long>    _memcpy_count;
		static unsigned long get_conenction_count();
		static HDS_UDP_Simple_Connection_Reader* malloc();
		void construct();
	};
}
#endif