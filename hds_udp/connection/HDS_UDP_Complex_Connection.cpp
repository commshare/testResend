#include "HDS_UDP_Complex_Connection.h"
#include "../common/HDS_WriteCDR.h"
#include "../common/HDS_ReadCDR.h"
#include "../method_processor/HDS_UDP_Method_Processor.h"
#include "../buffer/HDS_Buffer_Pool.h"
#include <stdio.h>
#include <stdlib.h>


namespace HDS_UDP
{
	static atomic_t<unsigned long> _time_out_count=0;
	unsigned long HDS_UDP_Complex_Connection_Writer::get_time_out_count()
	{
		return _time_out_count.value();
	}

	static inline void _writer_congestion(HDS_UDP_Complex_Connection_Writer* writer)
	{
		unsigned long next_window_ssthresh = writer->_window_ssthresh>>1;
		if(next_window_ssthresh < HDS_UDP_PACKET_MAX_DATA_LENGTH)
			next_window_ssthresh=  HDS_UDP_PACKET_MAX_DATA_LENGTH;
		writer->_window_ssthresh = next_window_ssthresh;
		writer->_window_site = HDS_UDP_PACKET_MAX_DATA_LENGTH;
		writer->_right_window_cursor = 
		writer->_old_current_window_cursor = writer->_current_window_cursor;
#ifdef _USE_COMPLEX_FILE_WINDOW_INCREASE_FACTOR_
		writer->_ack_seq_window_increase_factor*=0.5f;
#endif
	}

	static void _writer_conneciton_do_time_out(connection* con, Connection_Timer_Type timer_type,long running_count,void* extend_data)
	{
		HDS_UDP_Complex_Connection_Writer *writer = reinterpret_cast<HDS_UDP_Complex_Connection_Writer*>(con);
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

#ifdef HDS_UDP_DEBUG
					printf("++++++++++time out++++++++++++++%d\n",running_count);
#endif

					set_connection_RTO_Double(writer);
					
					//调整窗口和阀值
					if(lock_connection_status_ack_mutex(writer)==0)
					{
						_writer_congestion(writer);
						unlock_connection_status_ack_mutex(writer);
					}
					set_connection_status_has_time_out(writer);
					writer->add_to_sender_queue();
				}
				break;
			}
		default:break;
		}
	}

#define process_sender_msg_bit(con) \
	if(reset_connection_status_has_time_out(writer)==1){\
	writer->_current_window_cursor = writer->_left_window_cursor.value();}\
	if(reset_connection_status_has_sack(writer) == 1){\
	writer->_current_window_cursor = writer->_left_window_cursor.value();\
	set_connection_status_in_process_sack(writer);}\
	if(reset_sack_has_new_ack(writer)==1){\
	writer->_current_window_cursor = writer->_old_current_window_cursor;}


	static void _writer_conneciton_do_send(connection* con,char * sender_buf)
	{
		HDS_UDP_Complex_Connection_Writer *writer = reinterpret_cast<HDS_UDP_Complex_Connection_Writer*>(con);
		if(test_connection_force_stop(writer)==1)
		{
			if(set_connection_trans_suspend(writer)==0)
			{
				writer->_connection_ID.suspend_connection_ID();
				writer->add_to_timer_queue(HDS_UDP_CONNECTION_MAX_SUSPEND_TIME,Connection_Suspend_Timer,writer);
			}
			return;
		}

		process_sender_msg_bit(writer);
		int send_count=0;
		unsigned int dt_note;
		MsgType msgtype;
		while(writer->_current_window_cursor< writer->_right_window_cursor.value()&&
			writer->_current_window_cursor< writer->_transmit_buf_length)
		{
			process_sender_msg_bit(writer);
			if(send_count>=16)
			{
				set_connection_reback_sender(writer);
				writer->add_to_timer_queue(writer->_RTO.value(),Sender_Keeping_Timer,NULL);
				return;
			}
			msgtype = HDS_UDP_TRANS_COMPLEX_FILE_DATA;
			if(writer->_current_window_cursor == 0)
			{
				msgtype = HDS_UDP_TRANS_COMPLEX_FILE_REQUIRE_DUPLICATE;
				if(set_connection_status_has_send_require(writer)==0)
					msgtype = HDS_UDP_TRANS_COMPLEX_FILE_REQUIRE;
				dt_note = con->_transmit_buf_length;
			}
			else
			{
				dt_note = writer->_current_window_cursor;
			}
			HDS_WriteCDR cdr(sender_buf);
			int con_clock;
			connection_clock(con_clock);
			CONNECTION_FILL_HEADER(msgtype,con->_connection_ID,con_clock,dt_note,cdr);
			CONNECTION_FILL_DATA(cdr,writer->_current_window_cursor ,writer);
			kernel_send(sender_buf,cdr.length(),0,&writer->_remote_addr,writer->_remote_addr_length);
			writer->_running_count++;
			send_count++;
			writer->_current_window_cursor+=HDS_UDP_PACKET_MAX_DATA_LENGTH;
		}
		writer->add_to_timer_queue(writer->_RTO.value(),Sender_Keeping_Timer,NULL);
		return;
	}

	static void _writer_connection_do_network_data(connection *con,const MsgType msgtype,unsigned int seq, int send_time,char* recv_data, int recv_data_length,Kernel_Recv_Data* data)
	{
		HDS_UDP_Complex_Connection_Writer *writer = reinterpret_cast<HDS_UDP_Complex_Connection_Writer*>(con);
		writer->_try_time=0;
		int res=0;
		unsigned long seq_interval = seq - writer->_left_window_cursor.value();

		set_connection_left_window_cursor(con,seq,res);
		if(res == -1)
		{
			return;
		}

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

		if(lock_connection_status_ack_mutex(writer)==1)
		{
			return;
		}

		set_connection_RTO(writer,send_time);

		int is_new_sack=0;
		if(writer->_sack_seq != seq)
		{
			writer->_sack_seq=seq;
			writer->_sack_same_count =1;
			if(seq >= writer->_current_window_cursor)
			{
				is_new_sack = reset_connection_status_in_process_sack(writer);
			}
		}
		else
		{
			writer->_sack_same_count++;
		}

		if(writer->_sack_same_count >=3)
		{
			//这个时候出现了sack丢失，需要重新设置
			_writer_congestion(writer);
			set_connection_status_has_sack(writer);
			
#ifdef HDS_UDP_DEBUG
			printf("sack win size =%d  (%d %%) %d rto=%d\n", 
				writer->_window_site,
				((writer->_left_window_cursor.value()/10000)*100/(writer->_transmit_buf_length/10000)),writer->_left_window_cursor.value(),
				writer->_RTO.value());
#endif 

		}
		else
		{
			//正常情况，可以直接增加窗口大小
			//需要调节窗口值
#ifdef _USE_COMPLEX_FILE_WINDOW_INCREASE_FACTOR_
			float next_ack_seq_window_increase_factor= writer->_ack_seq_window_increase_factor +=0.05f;
			if(next_ack_seq_window_increase_factor >= 1.0f)
			{
				next_ack_seq_window_increase_factor = 1.0f;
			}
			writer->_ack_seq_window_increase_factor = next_ack_seq_window_increase_factor;
#endif

			unsigned long next_window_size=0;
			if(is_new_sack)
			{
				writer->_window_site = writer->_window_ssthresh;
				set_sack_has_new_ack(writer);
			}
			else
			{
				next_window_size = writer->_window_site<<1;
				if(writer->_window_site >= writer->_window_ssthresh)
				{
#ifdef _USE_COMPLEX_FILE_WINDOW_INCREASE_FACTOR_
					next_window_size = writer->_window_site+ (unsigned long)(writer->_ack_seq_window_increase_factor * seq_interval);
#else
					next_window_size = writer->_window_site+seq_interval+HDS_UDP_PACKET_MAX_DATA_LENGTH;
#endif
					if(next_window_size > writer->_default_window_size)
					{
						next_window_size = writer->_default_window_size;
					}
					if(next_window_size < writer->_window_site)
					{
						//防止溢出
						next_window_size =  writer->_window_site;
					}
					writer->_window_ssthresh = next_window_size;
				}
				writer->_window_site =  next_window_size;
			}

			if(test_connection_status_in_process_sack(writer)!=1)
			{
				writer->_right_window_cursor= writer->_left_window_cursor.value() + writer->_window_site;
			}

#ifdef HDS_UDP_DEBUG
			printf("ack win size =%d  (%d %%) %d rto=%d\n", 
				writer->_window_site,
				((writer->_left_window_cursor.value()/10000)*100/(writer->_transmit_buf_length/10000)),writer->_left_window_cursor.value(),
				writer->_RTO.value());
#endif
		}
		
		unlock_connection_status_ack_mutex(writer);
		writer->add_to_sender_queue();
	}


	typedef HDS_Singleton_Pool<sizeof(HDS_UDP_Complex_Connection_Writer)>  _complex_connection_writer_pool;
	HDS_UDP_Complex_Connection_Writer* HDS_UDP_Complex_Connection_Writer::malloc()
	{
		return (HDS_UDP_Complex_Connection_Writer*)_complex_connection_writer_pool::malloc();;
	}

	static void writer_connection_free(connection* writer)
	{
		_complex_connection_writer_pool::free(writer);
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

	static unsigned long _get_writer_connection_type_count()
	{
		return _writer_connection_count.get_value();
	}

	unsigned long HDS_UDP_Complex_Connection_Writer::get_conenction_count()
	{
		return _writer_connection_count.get_value();
	}

	void HDS_UDP_Complex_Connection_Writer::construct()
	{
		memset(this,0,sizeof(HDS_UDP_Complex_Connection_Writer));
		this->_do_time_out = _writer_conneciton_do_time_out;
		this->_do_send      = _writer_conneciton_do_send;
		this->_do_network_data = _writer_connection_do_network_data;
		this->_connection_free_memory = writer_connection_free;
		this->_increase_connection_type_count = _writer_connection_type_count_increase;
		this->_decrease_conneciton_type_count = _writer_connection_type_count_decrease;
		this->_get_connection_type_count = _get_writer_connection_type_count;
		this->_window_ssthresh  = HDS_UDP_DEFAULT_SSTHRESH_SIZE;
		this->_right_window_cursor = HDS_UDP_PACKET_MAX_DATA_LENGTH;
		this->_window_site = HDS_UDP_PACKET_MAX_DATA_LENGTH;
#ifdef _USE_COMPLEX_FILE_WINDOW_INCREASE_FACTOR_
		this->_ack_seq_window_increase_factor = 1.0f;
#endif
	}


	static void _reader_conneciton_do_time_out(connection* con, Connection_Timer_Type timer_type,long running_count,void* extend_data)
	{
		HDS_UDP_Complex_Connection_Reader *reader = reinterpret_cast<HDS_UDP_Complex_Connection_Reader*>(con);
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
					return;
				}
				reader->add_to_timer_queue(HDS_UDP_CONNECTION_RECV_CHECK_DEAD_TIME,Recver_Check_Dead_Timer,NULL);
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
		case Recver_Succ_Timer:
			{
				if(set_connection_trans_succ(reader)==0)
				{
					HDS_UDP_Result_Process_Data process_data;;
					process_data.remote_addr_length = reader->_remote_addr_length;
					memcpy(&process_data.remote_addr,&reader->_remote_addr,reader->_remote_addr_length);
					process_data.data = reader->_transmit_buf;
					process_data.buf_type = reader->_recv_buf_type;
					process_data.data_length = reader->_transmit_buf_length;
					HDS_UDP_Method_Processor::push_msg(process_data);
				}
				break;
			}
		case Recver_ACK_Timer:
			{
				if(test_connection_force_stop(reader)==1||test_connection_trans_succ(reader)==1)
				{
					return;
				}
				unsigned int ack_seq;
				int send_time;
				Ack_Type type = reader->_ack_info_collector.get_next_ack(ack_seq,send_time);
				MsgType ack_msg_type = HDS_UDP_TRANS_COMPLEX_FILE_ACK;
				if(type == SACK)
				{
					ack_msg_type = HDS_UDP_TRANS_COMPLEX_FILE_SACK;
					set_reader_connection_sack(reader);
				}
				else
				{
					reset_reader_connection_sack(reader);
				}
				char recvdata[HDS_UDP_PACKET_MAX_HEADER_LENGTH];
				HDS_WriteCDR cdr(recvdata);
				CONNECTION_FILL_HEADER(
					ack_msg_type,
					reader->_connection_ID,
					send_time,
					ack_seq,
					cdr);
				kernel_send(recvdata,cdr.length(),0,&reader->_remote_addr,reader->_remote_addr_length);

				//trans successfully
				if(ack_seq > reader->_transmit_buf_length-1)
				{
					reader->_trans_result_code = TRANS_SUCCESSFULLY;
					if(set_reader_connection_status_recv_succ_timer(reader)==0)
					{
						reader->add_to_timer_queue(HDS_UDP_RECVER_SUCC_TIMER,Recver_Succ_Timer,NULL);
					}
				}
				else
				{
					reader->add_to_timer_queue(HDS_UDP_RECV_ACK_TIMER,Recver_ACK_Timer,NULL);
				}
				break;
			}
		}
	}

	static void _reader_connection_do_network_data(connection *con,const MsgType msgtype,unsigned int seq, 
		int send_time,char* recv_data, int recv_data_length,Kernel_Recv_Data* rdata)
	{
		HDS_UDP_Complex_Connection_Reader *reader = reinterpret_cast<HDS_UDP_Complex_Connection_Reader*>(con);
		if(test_connection_force_stop(reader)==1||test_connection_trans_succ(reader)==1)
		{
			unsigned int ack_seq;
			int send_time;
			Ack_Type type = reader->_ack_info_collector.get_next_ack(ack_seq,send_time);
			if((ack_seq - reader->_transmit_buf_length )<= HDS_UDP_PACKET_MAX_DATA_LENGTH)
			{
				HDS_WriteCDR cdr(rdata->data);
				CONNECTION_FILL_HEADER(HDS_UDP_TRANS_COMPLEX_FILE_ACK,
					reader->_connection_ID,send_time,ack_seq,cdr);
				kernel_send(rdata->data,cdr.length(),0,&reader->_remote_addr,reader->_remote_addr_length);
			}
			else
			{
				HDS_WriteCDR cdr(rdata->data);
				CONNECTION_FILL_HEADER(HDS_UDP_TRANS_RESET,
					reader->_connection_ID,send_time,ack_seq,cdr);
				kernel_send(rdata->data,cdr.length(),0,&reader->_remote_addr,reader->_remote_addr_length);
			}
			return;
		}

		reader->_running_count++;
		int bit_value=0;
		if(reader->_ack_info_collector.set(seq,send_time,&bit_value)==-1)
		{
			return;
		}

		if(bit_value == 1)
			return;

		memcpy(reader->_transmit_buf+seq,recv_data,recv_data_length);
	}


	typedef HDS_Singleton_Pool<sizeof(HDS_UDP_Complex_Connection_Reader)>  _complex_connection_reader_pool;
	HDS_UDP_Complex_Connection_Reader* HDS_UDP_Complex_Connection_Reader::malloc()
	{
		return (HDS_UDP_Complex_Connection_Reader*)_complex_connection_reader_pool::malloc();;
	}

	static void _reader_connection_free(connection* reader)
	{
		_complex_connection_reader_pool::free(reader);
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

	unsigned long HDS_UDP_Complex_Connection_Reader::get_conenction_count()
	{
		return _reader_connection_count.get_value();
	}

	void HDS_UDP_Complex_Connection_Reader::construct()
	{
		memset(this,0,sizeof(HDS_UDP_Complex_Connection_Reader));
		this->_do_time_out = _reader_conneciton_do_time_out;
		this->_do_network_data = _reader_connection_do_network_data;
		this->_connection_free_memory = _reader_connection_free;
		this->_increase_connection_type_count = _reader_conenciton_type_increase_count;
		this->_decrease_conneciton_type_count = _reader_conenciton_type_decrease_count;
		this->_get_connection_type_count = _get_reader_connection_type_count;
	}
}