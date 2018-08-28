#include "HDS_UDP.h"
#include "../kernel/HDS_UDP_Kernel.h"
#include "../method_processor/HDS_UDP_Method_Processor.h"
#include "../connection/HDS_UDP_Connection.h"
#include "../connection/HDS_UDP_Connection_Timer.h"
#include "../connection/HDS_UDP_Connection_Sender.h"
#include "../connection/HDS_UDP_Connection_Recv_Wait_ACK_Queue.h"
#include "../connection/HDS_UDP_Complex_Connection.h"
#include "../connection/HDS_UDP_Simple_Connection.h"
#include "../buffer/HDS_Buffer_Pool.h"


namespace HDS_UDP
{
	//初始化网络库
	int HDS_UDP_EXPORT_API init_HDS_UDP(
		const char* ipaddr_,
		const unsigned short port_, 
		const int udp_packet_filter_thread_num, 
		const int process_msg_thread_num,
		const int timer_thread_thread_num,
		const int sender_thread_num)
	{
		if(Buffer_Pool::init()==-1)
			return -1;

		if(Recv_Wait_Ack_Queue::init() == -1)
			return -1;

		if(connection::init()==-1)
			return -1;

		if(init_kernel(ipaddr_,port_,udp_packet_filter_thread_num)==-1)
			return -1;

		if(HDS_UDP_Method_Processor::init_processor(process_msg_thread_num)==-1)
			return -1;

		Sleep(200);
		if(Timer::init_timer_queue(timer_thread_thread_num)==-1)
			return -1;

		if(Sender::init_sender(sender_thread_num) == -1)
			return -1;

		return 0;
	}

	//关闭网络库
	int HDS_UDP_EXPORT_API fini_HDS_UDP()
	{
		if(HDS_UDP_Method_Processor::fini_porcessor()==-1)
			return -1;
		if(Sender::fini_sender()==-1)
			return -1;
		if(Timer::fini_timer_queue()==-1)
			return -1;
		if(fini_kernel() == -1)
			return -1;
		if(connection::fini())
			return -1;
		if(Recv_Wait_Ack_Queue::fini() == -1)
			return -1;
		if(Buffer_Pool::fini() == -1)
			return -1;
		return 0;
	}

	//注册消息类型，与相应的消息处理类绑定
	void  HDS_UDP_EXPORT_API regeditProcessor(const MsgType type,  const PI_UDP_Processor  pProcessor)
	{
		HDS_UDP_Method_Processor::registe_method(type,pProcessor);
	}

#ifdef _SUPPORT_ACE_
	extern "C"
		int  HDS_UDP_EXPORT_API  send(ACE_Message_Block * block , ACE_INET_Addr& ace_addr , 
		HDS_UDP_SENDED_DEL_TYPE del_type)
	{
		SOCKADDR_IN remoteAddr;
		remoteAddr.sin_family=AF_INET;  //这个值对以后会有影响
		remoteAddr.sin_addr.s_addr=htonl(ace_addr.get_ip_address());// ace_remoteAddr.get_ip_address();
		remoteAddr.sin_port=htons(ace_addr.get_port_number());//ace_remoteAddr.get_port_number();
		int res = kernel_send(block->rd_ptr(),block->length(),0,(sockaddr*)&remoteAddr,sizeof(SOCKADDR_IN));
		if(del_type == DELETE_AFTER_SEND)
		{
			block->release();
		}
		return res;
	}
#else
	int  HDS_UDP_EXPORT_API  send(char * block, unsigned int length , sockaddr * addr , socklen_t addr_length, 
		HDS_UDP_SENDED_DEL_TYPE del_type)
	{
		return kernel_send(block,length,0,(sockaddr*)&addr,addr_length);
	}
#endif


	int HDS_UDP_EXPORT_API Post_Send(char*block,unsigned int length,sockaddr_in & remote_addr,
		HDS_UDP_SENDED_DEL_TYPE del_type)
	{
		return kernel_send(block,length,0,(sockaddr*)&remote_addr,sizeof(sockaddr_in));
	}


#ifdef _SUPPORT_ACE_
	int HDS_UDP_EXPORT_API query_separate_recv_buf_AceBlock(ACE_Message_Block* block)
	{
		return HDS_UDP_Method_Processor::query_separate_recv_buf_AceBlock(block);
	}
#else
	int HDS_UDP_EXPORT_API query_separate_recv_buf(HDS_UDP_Result_Process_Data * data)
	{
		return HDS_UDP_Method_Processor::query_separate_recv_buf(data);
	}
#endif


	int  HDS_UDP_EXPORT_API  C_RSendLink(
		message_node * node,
		SOCKADDR_IN & remote_addr,
		Connection_ID&con_id,
		unsigned long defaultSSTHRESH, 
		unsigned long maxWindowSize
		)
	{
		return connection::start_link_send(node,(sockaddr*)&remote_addr,sizeof(sockaddr_in),&con_id,defaultSSTHRESH,maxWindowSize);
	}


	int HDS_UDP_EXPORT_API Send_Link
		(
		message_node * node,
		sockaddr * remote_addr,
		socklen_t    remote_addr_length,
		Connection_ID *con_id,
		unsigned long defaultSSTHRESH, 
		unsigned long  maxWindowSize
		)
	{
		return connection::start_link_send(node,remote_addr,remote_addr_length,con_id,defaultSSTHRESH,maxWindowSize);
	}


	int HDS_UDP_EXPORT_API CSend(
		char * block , 
		unsigned int length ,
		sockaddr_in& addr , 
		HDS_UDP_SENDED_DEL_TYPE del_type,HDS_UDP_Sender_Handler handler
		)
	{
		return connection::start_sender(NULL, block, length,
			(sockaddr*)&addr,sizeof(sockaddr_in),NULL,del_type,handler,HDS_UDP_DEFAULT_SSTHRESH_SIZE,-1);
	}

	extern "C"
		int  HDS_UDP_EXPORT_API  C_RSend(
		char * block,
		unsigned int length,
		sockaddr_in & remote_addr,
		Connection_ID&con_id,
		HDS_UDP_SENDED_DEL_TYPE del_type,
		HDS_UDP_Sender_Handler  handler,
		unsigned long defaultSSTHRESH, 
		unsigned long  maxWindowSize
		)
	{
		return connection::start_sender(NULL, block, length,
			(sockaddr*)&remote_addr,sizeof(SOCKADDR_IN), &con_id,del_type,handler,defaultSSTHRESH,maxWindowSize);
	}

#ifdef _SUPPORT_ACE_
	int  HDS_UDP_EXPORT_API  R_Send(
		char *block,
		unsigned int length,
		ACE_INET_Addr & ace_addr,
		Connection_ID&con_id,
		HDS_UDP_SENDED_DEL_TYPE del_type,
		HDS_UDP_Sender_Handler  handler,
		unsigned long defaultSSTHRESH, 
		unsigned long  maxWindowSize)
	{
		SOCKADDR_IN remoteAddr;
		remoteAddr.sin_family=AF_INET;  //这个值对以后会有影响
		remoteAddr.sin_addr.s_addr=htonl(ace_addr.get_ip_address());// ace_remoteAddr.get_ip_address();
		remoteAddr.sin_port=htons(ace_addr.get_port_number());//ace_remoteAddr.get_port_number();
		return connection::start_sender(NULL,block,length,(sockaddr*)&remoteAddr,sizeof(SOCKADDR_IN),
			&con_id,del_type,handler,defaultSSTHRESH,maxWindowSize);

	}
#endif


#ifdef _SUPPORT_ACE_
	/*支持ace的可靠传输，为兼容以前的版本*/
	extern "C"
		int  HDS_UDP_EXPORT_API  send_rudp_s_file_f(ACE_Message_Block * block , 
		ACE_INET_Addr& addr,Connection_ID & f_id , 
		HDS_UDP_SENDED_DEL_TYPE del_type ,
		HDS_UDP_Sender_Handler handler)
	{
		return connection::start_sender_ace(block,addr,&f_id,del_type,handler,HDS_UDP_DEFAULT_SSTHRESH_SIZE,-1);
	}

	/*支持ace的可靠传输，为兼容以前的版本*/
	int  HDS_UDP_EXPORT_API  send_rudp_s_file_nf(ACE_Message_Block * block , 
		ACE_INET_Addr& addr, 
		HDS_UDP_SENDED_DEL_TYPE del_type,
		HDS_UDP_Sender_Handler handler)
	{
		Connection_ID f_id;
		return connection::start_sender_ace(block,addr,&f_id,del_type,handler,HDS_UDP_DEFAULT_SSTHRESH_SIZE,-1);
	}

	int  HDS_UDP_EXPORT_API  send_rudp_need_file_id(
		ACE_Message_Block *block,
		ACE_INET_Addr & addr,
		Connection_ID &f_id,  
		HDS_UDP_SENDED_DEL_TYPE del_type,
		HDS_UDP_Sender_Handler  handler,
		unsigned long defaultSSTHRESH, 
		unsigned long maxWindowSize)
	{
		return connection::start_sender_ace(block,addr,&f_id,del_type,handler,defaultSSTHRESH,maxWindowSize);
	}

#endif

	int HDS_UDP_EXPORT_API get_connection_info(CONNECTION_INFO& info, Connection_ID & con_ID)
	{
		return connection::get_connection_info(&info,&con_ID);
	}

	//获取正在接收的小文件连接的数目
	int HDS_UDP_EXPORT_API get_recv_small_con_num()
	{
		return  connection::get_simple_connection_reader_num();
	}

	//获取正在接收的大文件连接的数目
	int HDS_UDP_EXPORT_API get_recv_big_con_num()
	{
		return connection::get_complex_connection_reader_num();
	}

	//获取正在发送的小文件的连接数目
	int HDS_UDP_EXPORT_API get_send_small_con_num()
	{
		return connection::get_simple_connection_writer_num();
	}

	//获取正在发送的大文件的连接数目
	int HDS_UDP_EXPORT_API get_send_big_con_num()
	{
		return connection::get_complex_conenction_writer_num();
	}

	void HDS_UDP_EXPORT_API set_big_file_reader_must_crrecv()
	{
		Connection_Reader::set_complex_file_need_verification();
	}

	void HDS_UDP_EXPORT_API reset_big_file_reader_must_crrecv()
	{
		Connection_Reader::reset_complex_file_need_verification();
	}

	int HDS_UDP_EXPORT_API stop_connection(HDS_UDP_Connection_ID * con_ID)
	{
		return connection::connection_do_connection_stop(con_ID);
	}

	void HDS_UDP_EXPORT_API set_recv_verification_func(HDS_UDP_Recv_Verification_Func func)
	{
		return connection::set_recv_verification_func(func);
	}

	unsigned short HDS_UDP_EXPORT_API get_udp_port()
	{
		unsigned short port=0;
		get_udp_server_udp_port(port);
		return port;
	}

	unsigned long HDS_UDP_EXPORT_API get_downloaded_byte_count()
	{
		return get_udp_server_downloaded_byte_count();
	}

	//获取一共上传的字节数，注意会溢出的
	unsigned long HDS_UDP_EXPORT_API get_uploaded_byte_count()
	{
		return get_udp_server_uploaded_byte_count();
	}

	//获取接收超时的累积次数，注意会溢出
	unsigned long HDS_UDP_EXPORT_API get_timeout_count()
	{
		return HDS_UDP_Complex_Connection_Writer::get_time_out_count()+HDS_UDP_Simple_Connection_Writer::get_time_out_count();
	}
}

