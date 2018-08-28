#include "HDS_UDP_Simple_Connection.h"
#include "../common/HDS_WriteCDR.h"
#include "../common/HDS_ReadCDR.h"
#include "../method_processor/HDS_UDP_Method_Processor.h"
#include "../buffer/HDS_Buffer_Pool.h"
#include <stdio.h>
namespace HDS_UDP
{
	static atomic_t<unsigned long> _time_out_count=0;
	unsigned long HDS_UDP_Simple_Connection_Writer::get_time_out_count()
	{
		return _time_out_count.value();
	}

	static void _writer_conneciton_do_time_out(connection* con, Connection_Timer_Type timer_type,long running_count,void* extend_data)
	{
		HDS_UDP_Simple_Connection_Writer *writer = reinterpret_cast<HDS_UDP_Simple_Connection_Writer*>(con);

		switch(timer_type)
		{
		case Connection_Suspend_Timer:
			{
				if(set_connection_trans_succ(writer)==0)
				{
					writer->writer_do_connection_sended_handle_and_clean_data();
					writer->_connection_ID.release_connection_ID();
					writer->free_connetcion();
				}
				break;
			}
		case Sender_Keeping_Timer:
			{
				if(writer->_running_count.value() == running_count)
				{
					_time_out_count++;
					if(++ writer->_try_time == HDS_UDP_CONNECTION_SEND_TRY_TIME)
					{
						//need to close this conneciton
						writer->_trans_result_code = TRANS_TIME_OUT;
						if(set_connection_trans_suspend(writer)==0)
						{
							writer->_connection_ID.suspend_connection_ID();
							writer->add_to_timer_queue(HDS_UDP_CONNECTION_MAX_SUSPEND_TIME,Connection_Suspend_Timer,writer);
						}
						return;
					}
					set_connection_RTO_Double(writer);
					writer->add_to_sender_queue();
				}
				break;
			}
		default:break;
		}
	}

	static void _writer_conneciton_do_send(connection* con,char * sender_buf)
	{
		HDS_UDP_Simple_Connection_Writer *writer = reinterpret_cast<HDS_UDP_Simple_Connection_Writer*>(con);
		if(test_connection_force_stop(writer)==1)
		{
			if(set_connection_trans_suspend(writer)==0)
			{
				writer->_connection_ID.suspend_connection_ID();
				writer->add_to_timer_queue(HDS_UDP_CONNECTION_MAX_SUSPEND_TIME,Connection_Suspend_Timer,writer);
			}
			return;
		}

		unsigned int seq = writer->_left_window_cursor.value();
		unsigned int dt_note = seq;

		if(seq > writer->_transmit_buf_length-1)
			return;
		 
		MsgType msgtype = HDS_UDP_TRANS_SIMPLE_FILE_DATA;
		if(seq == 0)
		{
			msgtype = HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE_DUPLICATE;
			if(set_connection_status_has_send_require(writer)==0)
				msgtype = HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE;
			dt_note = con->_transmit_buf_length;
		}
		HDS_WriteCDR cdr(sender_buf);
		int con_clock;
		connection_clock(con_clock);
		CONNECTION_FILL_HEADER(msgtype,con->_connection_ID,con_clock,dt_note,cdr);
		CONNECTION_FILL_DATA(cdr,seq,writer);
		kernel_send(sender_buf,cdr.length(),0,&writer->_remote_addr,writer->_remote_addr_length);
		writer->_running_count++;
		writer->add_to_timer_queue(writer->_RTO.value(),Sender_Keeping_Timer,NULL);
	}

	static void _writer_connection_do_network_data(connection *con,const MsgType msgtype,unsigned int seq, int send_time,char* recv_data, int recv_data_length,Kernel_Recv_Data * rdata)
	{
		HDS_UDP_Simple_Connection_Writer *writer = reinterpret_cast<HDS_UDP_Simple_Connection_Writer*>(con);
		writer->_try_time=0;
		int res=0;

		set_connection_left_window_cursor(con,seq,res);
		if(res == -1)
			return;

		writer->_running_count++;

		if(seq > writer->_transmit_buf_length-1)
		{
			writer->_trans_result_code = TRANS_SUCCESSFULLY;
			if(set_connection_trans_suspend(writer)==0)
			{
				writer->_connection_ID.suspend_connection_ID();
				writer->add_to_timer_queue(HDS_UDP_CONNECTION_MAX_SUSPEND_TIME,Connection_Suspend_Timer,writer);
			}
			return;
		}
		set_connection_RTO(writer,send_time);
		writer->add_to_sender_queue();
	}


	typedef HDS_Singleton_Pool<sizeof(HDS_UDP_Simple_Connection_Writer)>  _simple_connection_writer_pool;
	HDS_UDP_Simple_Connection_Writer* HDS_UDP_Simple_Connection_Writer::malloc()
	{
		return (HDS_UDP_Simple_Connection_Writer*)_simple_connection_writer_pool::malloc();
	}

	static void _writer_connection_free(connection* con)
	{
		_simple_connection_writer_pool::free(con);
	}

	static atomic_t<unsigned long> _writer_connection_count;

	static void _writer_connection_type_count_increase()
	{
		_writer_connection_count++;
	}

	static void _writer_connection_type_count_decrease()
	{
		_writer_connection_count--;
	}

	unsigned long HDS_UDP_Simple_Connection_Writer::get_conenction_count()
	{
		return _writer_connection_count.get_value();
	}

	static unsigned long _get_writer_connection_type_count()
	{
		return _writer_connection_count.get_value();
	}

    void HDS_UDP_Simple_Connection_Writer::construct()
	{
		memset(this,0,sizeof(HDS_UDP_Simple_Connection_Writer));
		this->_do_time_out = _writer_conneciton_do_time_out;
		this->_do_send      = _writer_conneciton_do_send;
		this->_do_network_data = _writer_connection_do_network_data;
		this->_connection_free_memory = _writer_connection_free;
		this->_increase_connection_type_count = _writer_connection_type_count_increase;
		this->_decrease_conneciton_type_count = _writer_connection_type_count_decrease;
		this->_get_connection_type_count = _get_writer_connection_type_count;
	}

	static void _reader_conneciton_do_time_out(connection* con,Connection_Timer_Type timer_type,long running_count,void* extend_data)
	{
		HDS_UDP_Simple_Connection_Reader *reader = reinterpret_cast<HDS_UDP_Simple_Connection_Reader*>(con);

		switch(timer_type)
		{
		case Force_Reset_Timer:
			{
				if(set_connection_trans_succ(reader)==0)
				{
					reader->reader_do_connection_sended_handle_and_clean_data();
				}
				break;
			}
		case Recver_Check_Dead_Timer:
			{
				if(reader->_running_count.value() == running_count)
				{
					//可以删除此连接了
					reader->_trans_result_code = TRANS_TIME_OUT;
					if(set_connection_trans_suspend(reader)==0)
					{
						reader->_connection_ID.suspend_connection_ID();
						reader->add_to_timer_queue(HDS_UDP_CONNECTION_MAX_SUSPEND_TIME,Connection_Suspend_Timer,reader);
					}
				}
				else
				{
					reader->add_to_timer_queue(HDS_UDP_CONNECTION_RECV_CHECK_DEAD_TIME,Recver_Check_Dead_Timer,NULL);
				}
				break;
			}
		case Connection_Suspend_Timer:
			{
				if(set_connection_trans_succ(reader)==0)
				{
					reader->reader_do_connection_sended_handle_and_clean_data();
				}
				reader->_connection_ID.release_connection_ID();
				reader->free_connetcion();
				break;
			}
		}
	}

	static void _reader_connection_do_network_data(connection *con,const MsgType msgtype,unsigned int seq, int send_time,char* recv_data,
		int recv_data_length,Kernel_Recv_Data * rdata)
	{
		HDS_UDP_Simple_Connection_Reader *reader = reinterpret_cast<HDS_UDP_Simple_Connection_Reader*>(con);
		if(test_connection_force_stop(reader)==1||test_connection_trans_succ(reader)==1)
		{
			if((reader->_left_window_cursor.value() - reader->_transmit_buf_length )<= HDS_UDP_PACKET_MAX_DATA_LENGTH)
			{
				//trans succefully 
				HDS_WriteCDR cdr(rdata->data);
				CONNECTION_FILL_HEADER(HDS_UDP_TRANS_SIMPLE_FILE_ACK,
					reader->_connection_ID,send_time,reader->_left_window_cursor.value(),cdr);
				kernel_send(rdata->data,cdr.length(),0,&reader->_remote_addr,reader->_remote_addr_length);
			}
			else
			{
				HDS_WriteCDR cdr(rdata->data);
				CONNECTION_FILL_HEADER(HDS_UDP_TRANS_RESET,reader->_connection_ID,send_time,seq,cdr);
				kernel_send(rdata->data,cdr.length(),0,&reader->_remote_addr,reader->_remote_addr_length);
			}
			return;
		}

		reader->_running_count++;
		if(reader->_left_window_cursor.CompareExchange(seq+HDS_UDP_PACKET_MAX_DATA_LENGTH,seq) == seq)
		{
			memcpy(reader->_transmit_buf+seq,recv_data,recv_data_length);
			reader->_memcpy_count++;

			if(reader->_left_window_cursor.value() > reader->_transmit_buf_length-1
				&&reader->_memcpy_count.value() == (long)((reader->_transmit_buf_length-1)/HDS_UDP_PACKET_MAX_DATA_LENGTH+1))
			{
				if(set_connection_trans_succ(reader)==0)
				{
					con->_trans_result_code = TRANS_SUCCESSFULLY;
					HDS_UDP_Result_Process_Data * process_data = (HDS_UDP_Result_Process_Data*)(rdata);
					process_data->data = reader->_transmit_buf;
					process_data->buf_type = reader->_recv_buf_type;
					process_data->data_length = reader->_transmit_buf_length;
					HDS_UDP_Method_Processor::push_msg(*process_data);
				}
			}
		}
		//发送确认
		HDS_WriteCDR cdr(rdata->data);
		CONNECTION_FILL_HEADER(
			HDS_UDP_TRANS_SIMPLE_FILE_ACK,
			reader->_connection_ID,send_time,
			reader->_left_window_cursor.value(),
			cdr);
		kernel_send(rdata->data,cdr.length(),0,&reader->_remote_addr,reader->_remote_addr_length);
	}

	typedef HDS_Singleton_Pool<sizeof(HDS_UDP_Simple_Connection_Reader)>  _simple_connection_reader_pool;
	HDS_UDP_Simple_Connection_Reader* HDS_UDP_Simple_Connection_Reader::malloc()
	{
		return (HDS_UDP_Simple_Connection_Reader*) _simple_connection_reader_pool::malloc();
	}

	static void _reader_connection_free(connection* con)
	{
		_simple_connection_reader_pool::free(con);
	}

	static atomic_t<unsigned long> _reader_connection_count=0;
	static void _reader_conenciton_type_increase_count()
	{
		_reader_connection_count++;
	}

	static void _reader_conenciton_type_decrease_count()
	{
		_reader_connection_count--;
	}

	static unsigned long _get_reader_connection_type_count()
	{
		return _reader_connection_count.get_value();
	}
	
	unsigned long HDS_UDP_Simple_Connection_Reader::get_conenction_count()
	{
		return _reader_connection_count.get_value();
	}

	void HDS_UDP_Simple_Connection_Reader::construct()
	{
		memset(this,0,sizeof(HDS_UDP_Simple_Connection_Reader));
		this->_do_time_out = _reader_conneciton_do_time_out;
		this->_do_network_data = _reader_connection_do_network_data;
		this->_connection_free_memory = _reader_connection_free;
		this->_increase_connection_type_count = _reader_conenciton_type_increase_count;
		this->_decrease_conneciton_type_count = _reader_conenciton_type_decrease_count;
		this->_get_connection_type_count = _get_reader_connection_type_count;
	};
}