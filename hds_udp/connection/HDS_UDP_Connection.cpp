#include "HDS_UDP_Connection.h"
#include "../common/Atomic_Bitmap.h"
#include "../config/HDS_UDP_Kernel_Config.h"
#include "../common/HDS_ReadCDR.h"
#include "../common/HDS_WriteCDR.h"
#include "../common/HDS_Timer_Wheel.h"
#include "../method_processor/HDS_UDP_Method_Processor.h"
#include "HDS_UDP_Connection_Timer.h"
#include "../buffer/HDS_Buffer_Pool.h"
#include "HDS_UDP_Connection_Sender.h"
#include "HDS_UDP_Simple_Connection.h"
#include "HDS_UDP_Complex_Connection.h"
#include "HDS_UDP_Connection_Recv_Wait_ACK_Queue.h"
#include "../buffer/HDS_Buffer_Pool.h"
#include <stdio.h>

namespace HDS_UDP
{
	////////////////////////////////////////////////////////////////////////
	//数组0是不用的,按照现在的情况是，是可以修改验证位的大小的
	//+------------12----------------+-------------20--------------------------+
	//     前12位用于进行信息验证         后20位用于数组偏移
	///////////////////////////////////////////////////////////////////////
	static atomic_t<connection*>  _cid_array[HDS_UDP_MAX_CONNECTION_COUNT];
	static atomic_t<long>   _cid_array_offset=1;
	static atomic_t<unsigned long> _verification_actor=0xabcdefab;

	static atomic_t<long> _complex_file_need_verification=0;
#define  test_complex_file_need_verification() \
	(_complex_file_need_verification.value()>0)

	static HDS_UDP_Recv_Verification_Func _recv_verification_func=0;
	static atomic_t<long>        _recv_must_verification=0;
	void connection::set_recv_verification_func(HDS_UDP_Recv_Verification_Func func)
	{
		_recv_verification_func = func;
	}

	typedef HDS_Singleton_Pool<sizeof(_timer_parm_)> _timer_parm_pool;
	typedef HDS_Singleton_Pool<sizeof(message_node)> _message_node_pool;

#define CONNECTION_SUSPEND ((connection*)1)

	

	int connection::init()
	{
		timeval val;
		gettimeofday(&val,NULL);
		_cid_array_offset= (val.tv_usec)%HDS_UDP_MAX_CONNECTION_COUNT;
		_verification_actor = val.tv_sec;
		return 0;
	}

	int Connection_ID::create_connectionID(const void* connection_addr)
	{
		int offset = 0;
		int i=0;
		while(i<HDS_UDP_MAX_CONNECTION_COUNT)
		{
			offset = _cid_array_offset++;
			i++;
			_cid_array_offset.CompareExchange(1,HDS_UDP_MAX_CONNECTION_COUNT);
			if(_cid_array[offset].CompareExchange((connection*)connection_addr,0)==0)
				break;
			else
				return -1;
		}
		
		if(i>=HDS_UDP_MAX_CONNECTION_COUNT)
			return -1;

		unsigned long ver_value = _verification_actor++;
		this->_CID._ID32[0]= offset+ ((ver_value&0xfff)<<HDS_UDP_CONNECTION_MARK_BITS);
		return 0;
	}

	void Connection_ID::create_connectionID_label()
	{
		timeval val;
		gettimeofday(&val,NULL);
		this->_CID._ID32[1] =(((val.tv_usec>>10)&0x3ff) | (val.tv_sec&0xfffffc00));
	}


	//挂起connection_ID
	void Connection_ID::suspend_connection_ID()
	{
		_cid_array[this->_CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT].AtomicExchange(CONNECTION_SUSPEND);
	}

	//回收ID
	void Connection_ID::release_connection_ID()
	{
		_cid_array[this->_CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT].AtomicExchange((connection*)NULL);
	}

	void connection::filter_data(Kernel_Recv_Data* rdata)
	{
		HDS_ReaderCDR cdr(rdata->data);
		MsgType type;
		cdr.read_long(type);
		switch(type)
		{
		case HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE:
		case HDS_UDP_TRANS_COMPLEX_FILE_REQUIRE:
			{
				connection::start_recver(type,rdata);
				break;
			}
		case HDS_UDP_TRANS_SIMPLE_FILE_ACK:
		case HDS_UDP_TRANS_SIMPLE_FILE_DATA:
		case HDS_UDP_TRANS_COMPLEX_FILE_ACK:
		case HDS_UDP_TRANS_COMPLEX_FILE_DATA:
		case HDS_UDP_TRANS_COMPLEX_FILE_SACK:
			{
				connection::connection_do_network_data(type,rdata);
				break;
			}
		case HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE_DUPLICATE:
		case HDS_UDP_TRANS_COMPLEX_FILE_REQUIRE_DUPLICATE:
			{
				connection::connection_do_require_duplicate(type,rdata);
				break;
			}
		case HDS_UDP_TRANS_RESET:
		case HDS_UDP_TRANS_SIMPLE_FILE_REJECTED:
		case HDS_UDP_TRANS_COMPLEX_FILE_REJECTED:
			{
				connection::connection_do_connection_reset(type,rdata);
				break;
			}
		default:
			{
				char * tmbuf = (char*)Buffer_Pool::get_buffer(rdata->data_length);
				if(tmbuf == NULL)
				{
					return;
				}
				memcpy(tmbuf,rdata->data,rdata->data_length);
				HDS_UDP_Result_Process_Data * process_data = (HDS_UDP_Result_Process_Data*)(rdata);
				process_data->data = tmbuf;
				if(rdata->data_length <= HDS_BUFFER_POOL_MAX_CTL_SIZE)
					process_data->buf_type = POOL_SPACE;
				else
					process_data->buf_type = MALLOC_SPACE_DEL;
				HDS_UDP_Method_Processor::push_msg(*process_data);
			}
		}
	}

	connection* connection::get_connection(const int offset)
	{
		connection* con = _cid_array[offset].value();
		if(con == CONNECTION_SUSPEND)
			return NULL;
		return con;
	}

#define __set_connection_out_recv_wait_ack_queue(con,conID) \
	if(reset_reader_connection_status_recv_wait_ack_queue(con)==1)\
	{Connection_Reader*reader=(Connection_Reader*)con;\
	conID._CID._ID32[0] = reader->_require_duplicate_label;\
	Recv_Wait_Ack_Queue::erase(&conID);}


	void connection::free_connetcion()
	{
		if(test_connection_type_reader(this)==true)
		{
			__set_connection_out_recv_wait_ack_queue(this,this->_connection_ID);
		}
		this->_connection_free_memory(this);
		this->_decrease_conneciton_type_count();
	}

	int __start_sender(message_node* node, char * block , unsigned int block_length ,
		sockaddr* remote_addr,socklen_t remote_addr_length,Connection_ID *reConID,
		HDS_UDP_SENDED_DEL_TYPE del_type,HDS_UDP_Sender_Handler handler,
		unsigned long defaultSSTHRESH,
		unsigned long maxWindowSize)
	{
		Connection_Writer* base_writer_connection=NULL;
		if(block_length<= SMALL_FILE_MAX_SIZE)
		{
			HDS_UDP_Simple_Connection_Writer* writer = HDS_UDP_Simple_Connection_Writer::malloc();
			if(writer == NULL)
				return -1;
			writer->construct();
			base_writer_connection = reinterpret_cast<Connection_Writer*>(writer);
		}
		else
		{
			HDS_UDP_Complex_Connection_Writer* writer = HDS_UDP_Complex_Connection_Writer::malloc();
			if(writer == NULL)
				return -1;
			writer->construct();
			writer->_default_window_size = maxWindowSize;
			writer->_window_ssthresh = defaultSSTHRESH;
			base_writer_connection = reinterpret_cast<Connection_Writer*>(writer);
		}
		if(base_writer_connection->_connection_ID.create_connectionID(base_writer_connection) ==-1)
		{
			base_writer_connection->_connection_free_memory(base_writer_connection);
			return -1;
		}
		base_writer_connection->_increase_connection_type_count();
		*reConID = base_writer_connection->_connection_ID;
		base_writer_connection->_connection_ID.create_connectionID_label();
		base_writer_connection->_RTO= HDS_UDP_CONNECTION_DEFAULT_RTO;
		base_writer_connection->_sended_handle = handler;
		base_writer_connection->_del_type = del_type;
		base_writer_connection->_trans_message_node=node;
		base_writer_connection->_transmit_buf = block;
		base_writer_connection->_transmit_buf_length = block_length;
		base_writer_connection->set_remote_addr(remote_addr,remote_addr_length);
		set_connection_type_writer(base_writer_connection);
		return 0;
	}

	int connection::start_sender(message_node* node, char * block , unsigned int block_length ,
		sockaddr* remote_addr,socklen_t remote_addr_length,Connection_ID *reConID,
		HDS_UDP_SENDED_DEL_TYPE del_type,HDS_UDP_Sender_Handler handler,
		unsigned long defaultSSTHRESH,
		unsigned long maxWindowSize)
	{
		Connection_ID conID;
		int res = __start_sender(node,block,block_length,remote_addr,remote_addr_length,&conID,del_type,handler,defaultSSTHRESH,maxWindowSize);
		if(res == 0)
		{
			if(reConID != NULL)
			{
				*reConID = conID;
			}
			Connection_Writer * writer = (Connection_Writer*)_cid_array[conID._CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT].value();
			writer->add_to_sender_queue();
			return 0;
		}
		return -1;
	}

	int connection::start_link_send(message_node * msg_node,
		sockaddr* remote_addr,socklen_t remote_addr_length,Connection_ID* conID,
		unsigned long defaultSSTHRESH,
		unsigned long maxWindowSize)
	{
		unsigned int total_length=0;
		message_node* node =msg_node;
		message_node* new_msg_node=NULL;
		message_node* xnode = NULL;
		message_node* temp_node=NULL;
		while(node)
		{
			total_length+=node->source_data_length;
			temp_node = (message_node*)_message_node_pool::malloc();
			if(temp_node==NULL)
				return -1;
			*temp_node = *node;
			if(new_msg_node==NULL)
				new_msg_node= temp_node;
			else
				xnode->next = temp_node;

			xnode=temp_node;
			node= node->next;
		}
		xnode->next = NULL;
		return connection::start_sender(new_msg_node,NULL,total_length,
			remote_addr,remote_addr_length,conID,DELETE_AFTER_SEND,NULL,
			defaultSSTHRESH,maxWindowSize);
	}

#ifdef _SUPPORT_ACE_
	int connection::start_sender_ace(ACE_Message_Block* block,ACE_INET_Addr& ace_addr,Connection_ID* conID,
		HDS_UDP_SENDED_DEL_TYPE del_type,HDS_UDP_Sender_Handler handler,
		unsigned long defaultSSTHRESH,
		unsigned long maxWindowSize)
	{
		SOCKADDR_IN remoteAddr;
		remoteAddr.sin_family=AF_INET;  //这个值对以后会有影响
		remoteAddr.sin_addr.s_addr=htonl(ace_addr.get_ip_address());// ace_remoteAddr.get_ip_address();
		remoteAddr.sin_port=htons(ace_addr.get_port_number());//ace_remoteAddr.get_port_number();
		int res = __start_sender(NULL,block->rd_ptr(),block->length(),(sockaddr*)&remoteAddr,sizeof(SOCKADDR_IN),conID,del_type,handler,
			defaultSSTHRESH,maxWindowSize);
		if(res ==0)
		{
			Connection_Writer * writer = (Connection_Writer*)_cid_array[conID->_CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT].value();
			writer->_ace_transmit_block = block;
			writer->add_to_sender_queue();
		}
		return res;
	}
#endif

	void connection::start_recver(MsgType require_type, Kernel_Recv_Data* rdata)
	{
		Connection_Reader* base_reader_connection = NULL;
		if(require_type == HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE)
		{
			HDS_UDP_Simple_Connection_Reader * reader = HDS_UDP_Simple_Connection_Reader::malloc();
			if(reader == NULL)
			{
				return;
			}
			reader->construct();
			base_reader_connection = reader;
		}
		else
		{
			if(test_complex_file_need_verification()==1 && _recv_verification_func == NULL)
			{
				return; 
			}
			HDS_UDP_Complex_Connection_Reader * reader = HDS_UDP_Complex_Connection_Reader::malloc();
			if(reader == NULL)
			{
				return;
			}
			reader->construct();
			base_reader_connection = reader;
		}
		base_reader_connection->_increase_connection_type_count();
		set_connection_type_reader(base_reader_connection);

		int send_time;
		char* recv_data;
		int recv_data_length;
		HDS_ReaderCDR cdr(rdata->data);
		CONNECTION_UNFILL_HEADER(cdr,
			base_reader_connection->_connection_ID,
			send_time,
			base_reader_connection->_transmit_buf_length,
			recv_data,
			recv_data_length);

		if (base_reader_connection->_transmit_buf_length <= 0)
		{
			return;
		}


		Connection_ID wq_con_ID = base_reader_connection->_connection_ID;
		if(base_reader_connection->_connection_ID.create_connectionID(base_reader_connection)==-1)
		{
			base_reader_connection->_connection_ID.set_zero();
			goto recv_reject;
		}

		base_reader_connection->_require_duplicate_label= wq_con_ID._CID._ID32[0];
		Recv_Wait_Ack_Queue::insert(&wq_con_ID,get_connection_offset(base_reader_connection));
		base_reader_connection->set_remote_addr(&rdata->remote_addr,rdata->remote_addr_length);
		set_reader_connection_status_recv_wait_ack_queue(base_reader_connection);

		if(_recv_verification_func)
		{
			HDS_UDP_Recv_Verification_Attribute atti;
			atti = _recv_verification_func(recv_data,recv_data_length,base_reader_connection->_transmit_buf_length,
				&base_reader_connection->_connection_ID,
				&rdata->remote_addr,rdata->remote_addr_length);

			if(atti._recv_type == Verification_Recv_Type_No_Recv)
			{
				//用户不愿意接收此文件
				goto recv_reject;
			}

			if(atti._recv_buf != NULL)
			{
				base_reader_connection->_transmit_buf = atti._recv_buf;
				base_reader_connection->_recv_buf_type = USER_SPACE_DEL;
				if(atti._recv_buf_type == Verification_Recv_Buf_Type_Manual_Manger)
				{
					base_reader_connection->_recv_buf_type = USER_SPACE_NO_DEL;
				}
			}

			base_reader_connection->_recv_failed_handle = atti._recv_failed_func;
		}

		if(base_reader_connection->_transmit_buf == NULL)
		{
			base_reader_connection->_transmit_buf = (char*)Buffer_Pool::get_buffer(base_reader_connection->_transmit_buf_length);
			if(base_reader_connection->_transmit_buf == NULL)
			{
				goto recv_reject;
			}

			base_reader_connection->_recv_buf_type = POOL_SPACE;
			if(base_reader_connection->_transmit_buf_length >HDS_BUFFER_POOL_MAX_CTL_SIZE)
				base_reader_connection->_recv_buf_type = MALLOC_SPACE_DEL;
		}

		base_reader_connection->_do_network_data(base_reader_connection,require_type,0,send_time,recv_data,recv_data_length,rdata);
		base_reader_connection->add_to_timer_queue(HDS_UDP_CONNECTION_RECV_CHECK_DEAD_TIME,Recver_Check_Dead_Timer,NULL);
		if(require_type == HDS_UDP_TRANS_COMPLEX_FILE_REQUIRE)
		{
			base_reader_connection->add_to_timer_queue(HDS_UDP_RECV_ACK_TIMER,Recver_ACK_Timer,NULL);
		}
		return;
recv_reject:
		Connection_ID con_ID = base_reader_connection->_connection_ID;
		base_reader_connection->_connection_ID.release_connection_ID();//放弃ID
		MsgType  send_msg_type=HDS_UDP_TRANS_COMPLEX_FILE_REJECTED;
		if(require_type == HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE)
		{
			send_msg_type = HDS_UDP_TRANS_SIMPLE_FILE_REJECTED;
		}
		base_reader_connection->_connection_free_memory(base_reader_connection);
		base_reader_connection->_decrease_conneciton_type_count();
		//发送拒绝消息
		HDS_WriteCDR writer_cdr(rdata->data);
		CONNECTION_FILL_HEADER(send_msg_type,con_ID,send_time,send_time,writer_cdr);
		kernel_send(rdata->data,cdr.length(),0,&rdata->remote_addr,rdata->remote_addr_length);
	}

	inline void connection::add_to_timer_queue(long DueTime, Connection_Timer_Type timer_type, void* ex_data)
	{
		_timer_parm_ *parm = (_timer_parm_*)_timer_parm_pool::malloc();
		if(parm == NULL)
			return;

		parm->connection_offset = get_connection_offset(this);
		parm->running_count = this->_running_count.value();
		parm->timeout_type = timer_type;
		parm->extend_data = ex_data;
		parm->connection_addr = this;
		Timer::add_timer(DueTime,parm);
	}

	void connection::connection_do_time_out(_timer_parm_ *parm)
	{
		connection * con = _cid_array[parm->connection_offset].value();
		if(con == NULL)
			return;

		if(con == CONNECTION_SUSPEND)
			con = (connection*)parm->connection_addr;

		con->_do_time_out(con,parm->timeout_type,parm->running_count,parm->extend_data);
		_timer_parm_pool::free(parm);
	}

	void Connection_Writer::add_to_sender_queue()
	{
		if(set_connection_in_sender(this)==0)
		{
			Sender::add_to_send_queue(this->_connection_ID._CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT);
		}
		else
		{
			set_connection_reback_sender(this);
		}
	}

	void Connection_Writer::connection_do_sender(const int offset,char* sender_buf)
	{
		Connection_Writer * con = (Connection_Writer*)_cid_array[offset].value();
		if(con== CONNECTION_SUSPEND || con==NULL)
			return;

		reset_connection_reback_sender(con);
		con->_do_send(con,sender_buf);
		reset_connection_in_sender(con);
		if(reset_connection_reback_sender(con)==1)
		{
			con->add_to_sender_queue();
		}
	}

	void Connection_Writer::writer_do_connection_sended_handle_and_clean_data()
	{
		if(this->_trans_message_node)
		{
			//为链表模式
			message_node * temp_node = this->_trans_message_node;
			while(temp_node)
			{
				if(temp_node->send_handle)
				{
#ifdef _SUPPORT_HDS_UDP_2_
					temp_node->send_handle(temp_node->source_data,temp_node->source_data_length,
						*((sockaddr_in*)(&this->_remote_addr)),this->_trans_result_code);
#else
					temp_node->send_handle(temp_node->source_data,temp_node->source_data_length,
						&this->_remote_addr,this->_remote_addr_length,this->_trans_result_code,this->_del_type);
#endif
				}
				if(temp_node->del_type == DELETE_AFTER_SEND)
				{
					free(temp_node->source_data);
				}
				temp_node = temp_node->next;
			}

			while(this->_trans_message_node)
			{
				_message_node_pool::free(this->_trans_message_node);
				this->_trans_message_node = this->_trans_message_node->next;
			}
			this->_trans_message_node = NULL;
			return;
		}

		if(this->_sended_handle)
		{
#ifdef _SUPPORT_HDS_UDP_2_
			this->_sended_handle(this->_transmit_buf,this->_transmit_buf_length,
				*((sockaddr_in*)(&this->_remote_addr)),this->_trans_result_code);
#else
			this->_sended_handle(this->_transmit_buf,this->_transmit_buf_length,
				&this->_remote_addr,this->_remote_addr_length,this->_trans_result_code,this->_del_type);
#endif

			if(this->_del_type == DELETE_AFTER_SEND)
			{
#ifdef _SUPPORT_ACE_
				if(this->_ace_transmit_block!=NULL){this->_ace_transmit_block->release();}
				else{free(this->_transmit_buf);}
#else
				free(this->_transmit_buf);
#endif
			}
		}
	}

	inline void connection::connection_do_network_data(const MsgType msgtype,Kernel_Recv_Data* rdata)
	{
		Connection_ID conID;
		int send_time;
		unsigned int seq;
		char* recv_data;
		int recv_data_length;

		HDS_ReaderCDR cdr(rdata->data);
		CONNECTION_UNFILL_HEADER(cdr,
			conID,
			send_time,
			seq,
			recv_data,
			recv_data_length);
		connection* con = (connection*)_cid_array[conID._CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT].value();

		if(con && con!= CONNECTION_SUSPEND && con->_connection_ID._CID._ID32[0]==conID._CID._ID32[0])
		{
			if(seq == HDS_UDP_PACKET_MAX_DATA_LENGTH)
			{
				switch(msgtype)
				{
				case HDS_UDP_TRANS_SIMPLE_FILE_DATA:
				case HDS_UDP_TRANS_COMPLEX_FILE_DATA:
					{
						Connection_Reader *reader = (Connection_Reader*)con;
						conID._CID._ID32[0] = reader->_require_duplicate_label;
						__set_connection_out_recv_wait_ack_queue(con,conID);
						break;
					}
				case HDS_UDP_TRANS_SIMPLE_FILE_ACK:
				case HDS_UDP_TRANS_COMPLEX_FILE_ACK:
				case HDS_UDP_TRANS_COMPLEX_FILE_SACK:
					{
						con->_connection_ID._CID._ID32[1]=conID._CID._ID32[1];
						break;
					}
				}
			}
			con->_do_network_data(con,msgtype,seq,send_time,recv_data,recv_data_length,rdata);
		}
	}

	inline void connection::connection_do_require_duplicate(const MsgType msgtype,Kernel_Recv_Data* rdata)
	{
		Connection_ID conID;
		int send_time;
		unsigned int seq;
		char* recv_data;
		int recv_data_length;

		HDS_ReaderCDR cdr(rdata->data);
		CONNECTION_UNFILL_HEADER(cdr,
			conID,
			send_time,
			seq,
			recv_data,
			recv_data_length);
		
		int offset=0;
		if(Recv_Wait_Ack_Queue::find(&conID,&offset)==-1)
		{
			MsgType mt = msgtype;
			if(mt == HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE_DUPLICATE)
				mt = HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE;
			else
				mt = HDS_UDP_TRANS_COMPLEX_FILE_REQUIRE;

			HDS_WriteCDR cdr(rdata->data);
			cdr.write_long(mt);
			connection::start_recver(mt,rdata);
			return;
		}
		
		Connection_Reader* con = (Connection_Reader*)_cid_array[offset].value();
		if(con==NULL || con== CONNECTION_SUSPEND)
			return;

		if(con->_connection_ID._CID._ID32[1]==conID._CID._ID32[1]&& con->_require_duplicate_label == conID._CID._ID32[0]
		&&con->_left_window_cursor.value() == HDS_UDP_PACKET_MAX_DATA_LENGTH
			&&sockaddrcmp(&rdata->remote_addr,rdata->remote_addr_length,&con->_remote_addr,rdata->remote_addr_length)==0)
		{
				MsgType mt = msgtype;
				if(mt == HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE_DUPLICATE)
					mt = HDS_UDP_TRANS_SIMPLE_FILE_ACK;
				else
					mt = HDS_UDP_TRANS_COMPLEX_FILE_ACK;
				HDS_WriteCDR cdr(rdata->data);
				CONNECTION_FILL_HEADER(mt,con->_connection_ID,send_time,HDS_UDP_PACKET_MAX_DATA_LENGTH,cdr);
				kernel_send(rdata->data,cdr.length(),0,&con->_remote_addr,con->_remote_addr_length);
		}
	}

	inline void connection::connection_do_connection_reset(const MsgType msgtype, Kernel_Recv_Data* rdata)
	{
		Connection_ID conID;
		int send_time;
		unsigned int seq;
		char* recv_data;
		int recv_data_length;

		HDS_ReaderCDR cdr(rdata->data);
		CONNECTION_UNFILL_HEADER(cdr,
			conID,
			send_time,
			seq,
			recv_data,
			recv_data_length);

		connection* con = (connection*)_cid_array[conID._CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT].value();
		if(con && con!= CONNECTION_SUSPEND)
		{
			if(set_connection_force_stop(con) ==0)
			{
				if(msgtype == HDS_UDP_TRANS_RESET)
				{
					con->_trans_result_code = TRANS_RESET;
				}
				else
				{
					con->_trans_result_code = TRANS_REJECTED;
				}
				if(test_connection_type_writer(con)==true)
				{
					if(set_connection_trans_suspend(con)==0)
					{
						con->_connection_ID.suspend_connection_ID();
						con->add_to_timer_queue(HDS_UDP_CONNECTION_MAX_SUSPEND_TIME,Connection_Suspend_Timer,con);
					}
				}
				else
				{
					con->add_to_timer_queue(HDS_UDP_RESET_REACTION_TIME,Force_Reset_Timer,NULL);
				}
			}
		}
	}

	int connection::connection_do_connection_stop(Connection_ID * conID)
	{
		connection* con = (connection*)_cid_array[conID->_CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT].value();
		if(con==NULL || con == CONNECTION_SUSPEND)
			return -1;
		if(set_connection_force_stop(con)==0)
		{
			con->_trans_result_code = TRANS_FORCE_STOP;
			if(test_connection_type_writer(con)==true)
			{
				if(set_connection_trans_suspend(con)==0)
				{
					con->_connection_ID.suspend_connection_ID();
					con->add_to_timer_queue(HDS_UDP_CONNECTION_MAX_SUSPEND_TIME,Connection_Suspend_Timer,con);
				}
			}
			else
			{
				con->add_to_timer_queue(HDS_UDP_RESET_REACTION_TIME,Force_Reset_Timer,NULL);
			}

			char reset_msg[HDS_UDP_PACKET_MAX_HEADER_LENGTH];
			HDS_WriteCDR cdr(reset_msg);
			CONNECTION_FILL_HEADER(HDS_UDP_TRANS_RESET,con->_connection_ID,-1,0,cdr);
			kernel_send(reset_msg,cdr.length(),0,&con->_remote_addr,con->_remote_addr_length);
		}
		return 0;
	}

	void Connection_Reader::reader_do_connection_sended_handle_and_clean_data()
	{
		if(this->_trans_result_code !=TRANS_SUCCESSFULLY)
		{
			HDS_UDP_Result_Process_Data process_data;
			process_data.buf_type = this->_recv_buf_type;
			process_data.data = this->_transmit_buf;
			process_data.data_length=this->_transmit_buf_length;
			memcpy(&process_data.remote_addr,&this->_remote_addr,this->_remote_addr_length);
			process_data.remote_addr_length = this->_remote_addr_length;
			if(this->_recv_failed_handle)
			{
				this->_recv_failed_handle(&process_data,&this->_connection_ID,this->_trans_result_code);
			}
			switch(process_data.buf_type)
			{
			case USER_SPACE_NO_DEL:case MALLOC_SPACE_NO_DEL:{break;}
			case POOL_SPACE:{Buffer_Pool::release_buffer(process_data.data,process_data.data_length);}
			case MALLOC_SPACE_DEL:case USER_SPACE_DEL:{free(process_data.data);break;}
			}
		}
	}

	unsigned long connection::get_simple_connection_reader_num()
	{
		return HDS_UDP_Simple_Connection_Reader::get_conenction_count();
	}

	unsigned long connection::get_simple_connection_writer_num()
	{
		return HDS_UDP_Simple_Connection_Writer::get_conenction_count();
	}

	unsigned long connection::get_complex_connection_reader_num()
	{
		return HDS_UDP_Complex_Connection_Reader::get_conenction_count();
	}

	unsigned long connection::get_complex_conenction_writer_num()
	{
		return HDS_UDP_Complex_Connection_Writer::get_conenction_count();
	}

	int connection::get_connection_info(CONNECTION_INFO* info, Connection_ID* con_ID)
	{
		connection* con = (connection*)_cid_array[con_ID->_CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT].value();
		if(con==NULL || con == CONNECTION_SUSPEND)
			return -1;
		info->complete_size = con->_left_window_cursor.value();
		info->remote_addr_length = con->_remote_addr_length;
		memcpy(&info->remote_addr,&con->_remote_addr,con->_remote_addr_length);
		info->trans_result_code = con->_trans_result_code;
		info->total_size = con->_transmit_buf_length;
		return 0;
	}

	void Connection_Reader::set_complex_file_need_verification()
	{
		_complex_file_need_verification = 1;
	}

	void Connection_Reader::reset_complex_file_need_verification()
	{
		_complex_file_need_verification =0;
	}
}