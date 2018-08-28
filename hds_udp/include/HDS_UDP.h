#ifndef _HDS_UDP_H_
#define _HDS_UDP_H_

#include "../method_processor/HDS_UDP_Method_Processor.h"
#include "../connection/HDS_UDP_Connection.h"
#include "../common/HDS_ReadCDR.h"

namespace HDS_UDP
{
	extern "C"
		int HDS_UDP_EXPORT_API init_HDS_UDP(
		const char* ipaddr_,
		const unsigned short port_, 
		const int udp_packet_filter_thread_num=4, 
		const int process_msg_thread_num=4,
		const int timer_thread_thread_num=4,
		const int sender_thread_num=4);


	extern "C"
		int HDS_UDP_EXPORT_API fini_HDS_UDP();

	/*
	注册处理方法
	@MsgType 消息类型
	@pProcessor  消息相对于的处理类
	*/
	extern "C"
		void  HDS_UDP_EXPORT_API regeditProcessor(const MsgType  type,const PI_UDP_Processor pProcessor);


	//此函数用于消息处理时，希望改变buf的type，让其在数据处理后不用删除buf而留给自己使用
#ifdef _SUPPORT_ACE_
	extern "C"
		int HDS_UDP_EXPORT_API query_separate_recv_buf_AceBlock(ACE_Message_Block* block);
#else
	extern "C"
		int HDS_UDP_EXPORT_API query_separate_recv_buf(HDS_UDP_Result_Process_Data * buf);
#endif

	/*
	发送一条链表中的数据
	@node 数据链
	@remote_addr 远程地址
	@con_id   连接的id，可以根据此id查看连接的状况
	@del_type 发送完成后是否删除数据块
	@handler  发送完成后回调的操作
	@defaultSSTHRESH 滑动窗口的初始阀值，在网络状况未明的情况下最好使用默认值
	@maxWindowSize 初始端口的大小，在网络状况未明的情况下最好使用默认值
	*/
	extern "C"
		int  HDS_UDP_EXPORT_API  C_RSendLink(
		message_node * node,
		SOCKADDR_IN & remote_addr,
		Connection_ID&con_id,
		unsigned long defaultSSTHRESH = HDS_UDP_DEFAULT_SSTHRESH_SIZE, 
		unsigned long  maxWindowSize =  HDS_UDP_DEFAULT_MAX_WINDOWS_SIZE
		);

	extern "C"
		int HDS_UDP_EXPORT_API Send_Link
		(
		message_node * node,
		sockaddr * remote_addr,
		socklen_t    remote_addr_length,
		Connection_ID *con_id,
		unsigned long defaultSSTHRESH = HDS_UDP_DEFAULT_SSTHRESH_SIZE, 
		unsigned long  maxWindowSize =  HDS_UDP_DEFAULT_MAX_WINDOWS_SIZE
		);



	extern "C"
		int HDS_UDP_EXPORT_API CSend(
		char * block , 
		unsigned int length ,
		sockaddr_in& addr , 
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND,
		HDS_UDP_Sender_Handler handler = 0
		);

	extern "C"
		int  HDS_UDP_EXPORT_API  C_RSend(
		char * block,
		unsigned int length,
		SOCKADDR_IN & remote_addr,
		Connection_ID&con_id,
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND,
		HDS_UDP_Sender_Handler  handler = 0,
		unsigned long  defaultSSTHRESH = HDS_UDP_DEFAULT_SSTHRESH_SIZE, 
		unsigned long  maxWindowSize =  HDS_UDP_DEFAULT_MAX_WINDOWS_SIZE
		);

#ifdef _SUPPORT_ACE_
	extern "C"
		int  HDS_UDP_EXPORT_API  R_Send(
		char *block,
		unsigned int length,
		ACE_INET_Addr & remote_addr,
		Connection_ID&con_id,
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND,
		HDS_UDP_Sender_Handler  handler = 0,
		unsigned long defaultSSTHRESH = HDS_UDP_DEFAULT_SSTHRESH_SIZE, 
		unsigned long  maxWindowSize =  HDS_UDP_DEFAULT_MAX_WINDOWS_SIZE);
#endif


#ifdef _SUPPORT_ACE_
	/*
	发送简单文件，无需可靠传输,支持ACE
	*/
	extern "C"
		int  HDS_UDP_EXPORT_API  send(ACE_Message_Block * block , ACE_INET_Addr& addr , 
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND);
#else
	extern "C"
		//写漏了unsigned int length
	int  HDS_UDP_EXPORT_API  send(char * block , unsigned int length, sockaddr * addr , socklen_t addr_length, 
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND);
#endif


	extern "C"
		int HDS_UDP_EXPORT_API Post_Send(char*block,unsigned int length,sockaddr_in & remote_addr,
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND);

#ifdef _SUPPORT_ACE_
	/*支持ace的可靠传输，为兼容以前的版本*/
	extern "C"
		int  HDS_UDP_EXPORT_API  send_rudp_s_file_f(
		ACE_Message_Block * block , 
		ACE_INET_Addr& addr,
		Connection_ID & f_id , 
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND ,
		HDS_UDP_Sender_Handler handler =0);

	/*支持ace的可靠传输，为兼容以前的版本*/
	extern "C"
	int  HDS_UDP_EXPORT_API  send_rudp_s_file_nf(
		ACE_Message_Block * block , 
		ACE_INET_Addr& addr, 
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND ,
		HDS_UDP_Sender_Handler handler = 0);

	/*支持ace的可靠传输，为兼容以前的版本*/
	extern "C"
	int  HDS_UDP_EXPORT_API  send_rudp_need_file_id(
		ACE_Message_Block *block,
		ACE_INET_Addr & addr,
		Connection_ID &fid,  
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND,
		HDS_UDP_Sender_Handler  handler = 0,
		unsigned long defaultSSTHRESH = HDS_UDP_DEFAULT_SSTHRESH_SIZE, 
		unsigned long maxWindowSize =  HDS_UDP_DEFAULT_MAX_WINDOWS_SIZE);
#endif


	//获取正在接收的小文件连接的数目
	extern "C"
	int HDS_UDP_EXPORT_API get_recv_small_con_num();

	//获取正在接收的大文件连接的数目
	extern "C"
	int HDS_UDP_EXPORT_API get_recv_big_con_num();

	//获取正在发送的小文件的连接数目
	extern "C"
	int HDS_UDP_EXPORT_API get_send_small_con_num();

	//获取正在发送的大文件的连接数目
	extern "C"
	int HDS_UDP_EXPORT_API get_send_big_con_num();

	extern "C"
	void HDS_UDP_EXPORT_API set_big_file_reader_must_crrecv();

	extern "C"
	void HDS_UDP_EXPORT_API reset_big_file_reader_must_crrecv();

	//获取连接的状态信息
	extern "C"
		int HDS_UDP_EXPORT_API get_connection_info(CONNECTION_INFO& info, Connection_ID & con_ID);

	extern "C"
		int HDS_UDP_EXPORT_API stop_connection(HDS_UDP_Connection_ID * con_ID);

	extern "C"
		void HDS_UDP_EXPORT_API set_recv_verification_func(HDS_UDP_Recv_Verification_Func func);


	//获取接收超时的累积次数，注意会溢出
	extern "C"
		unsigned long HDS_UDP_EXPORT_API get_timeout_count();

	//获取一共下载的字节数，注意会溢出的
	extern "C"
		unsigned long HDS_UDP_EXPORT_API get_downloaded_byte_count();

	//获取一共上传的字节数，注意会溢出的
	extern "C"
		unsigned long HDS_UDP_EXPORT_API get_uploaded_byte_count();

	extern "C"
	unsigned short HDS_UDP_EXPORT_API get_udp_port();
}


#endif