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
	ע�ᴦ����
	@MsgType ��Ϣ����
	@pProcessor  ��Ϣ����ڵĴ�����
	*/
	extern "C"
		void  HDS_UDP_EXPORT_API regeditProcessor(const MsgType  type,const PI_UDP_Processor pProcessor);


	//�˺���������Ϣ����ʱ��ϣ���ı�buf��type�����������ݴ������ɾ��buf�������Լ�ʹ��
#ifdef _SUPPORT_ACE_
	extern "C"
		int HDS_UDP_EXPORT_API query_separate_recv_buf_AceBlock(ACE_Message_Block* block);
#else
	extern "C"
		int HDS_UDP_EXPORT_API query_separate_recv_buf(HDS_UDP_Result_Process_Data * buf);
#endif

	/*
	����һ�������е�����
	@node ������
	@remote_addr Զ�̵�ַ
	@con_id   ���ӵ�id�����Ը��ݴ�id�鿴���ӵ�״��
	@del_type ������ɺ��Ƿ�ɾ�����ݿ�
	@handler  ������ɺ�ص��Ĳ���
	@defaultSSTHRESH �������ڵĳ�ʼ��ֵ��������״��δ������������ʹ��Ĭ��ֵ
	@maxWindowSize ��ʼ�˿ڵĴ�С��������״��δ������������ʹ��Ĭ��ֵ
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
	���ͼ��ļ�������ɿ�����,֧��ACE
	*/
	extern "C"
		int  HDS_UDP_EXPORT_API  send(ACE_Message_Block * block , ACE_INET_Addr& addr , 
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND);
#else
	extern "C"
		//д©��unsigned int length
	int  HDS_UDP_EXPORT_API  send(char * block , unsigned int length, sockaddr * addr , socklen_t addr_length, 
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND);
#endif


	extern "C"
		int HDS_UDP_EXPORT_API Post_Send(char*block,unsigned int length,sockaddr_in & remote_addr,
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND);

#ifdef _SUPPORT_ACE_
	/*֧��ace�Ŀɿ����䣬Ϊ������ǰ�İ汾*/
	extern "C"
		int  HDS_UDP_EXPORT_API  send_rudp_s_file_f(
		ACE_Message_Block * block , 
		ACE_INET_Addr& addr,
		Connection_ID & f_id , 
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND ,
		HDS_UDP_Sender_Handler handler =0);

	/*֧��ace�Ŀɿ����䣬Ϊ������ǰ�İ汾*/
	extern "C"
	int  HDS_UDP_EXPORT_API  send_rudp_s_file_nf(
		ACE_Message_Block * block , 
		ACE_INET_Addr& addr, 
		HDS_UDP_SENDED_DEL_TYPE del_type =DELETE_AFTER_SEND ,
		HDS_UDP_Sender_Handler handler = 0);

	/*֧��ace�Ŀɿ����䣬Ϊ������ǰ�İ汾*/
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


	//��ȡ���ڽ��յ�С�ļ����ӵ���Ŀ
	extern "C"
	int HDS_UDP_EXPORT_API get_recv_small_con_num();

	//��ȡ���ڽ��յĴ��ļ����ӵ���Ŀ
	extern "C"
	int HDS_UDP_EXPORT_API get_recv_big_con_num();

	//��ȡ���ڷ��͵�С�ļ���������Ŀ
	extern "C"
	int HDS_UDP_EXPORT_API get_send_small_con_num();

	//��ȡ���ڷ��͵Ĵ��ļ���������Ŀ
	extern "C"
	int HDS_UDP_EXPORT_API get_send_big_con_num();

	extern "C"
	void HDS_UDP_EXPORT_API set_big_file_reader_must_crrecv();

	extern "C"
	void HDS_UDP_EXPORT_API reset_big_file_reader_must_crrecv();

	//��ȡ���ӵ�״̬��Ϣ
	extern "C"
		int HDS_UDP_EXPORT_API get_connection_info(CONNECTION_INFO& info, Connection_ID & con_ID);

	extern "C"
		int HDS_UDP_EXPORT_API stop_connection(HDS_UDP_Connection_ID * con_ID);

	extern "C"
		void HDS_UDP_EXPORT_API set_recv_verification_func(HDS_UDP_Recv_Verification_Func func);


	//��ȡ���ճ�ʱ���ۻ�������ע������
	extern "C"
		unsigned long HDS_UDP_EXPORT_API get_timeout_count();

	//��ȡһ�����ص��ֽ�����ע��������
	extern "C"
		unsigned long HDS_UDP_EXPORT_API get_downloaded_byte_count();

	//��ȡһ���ϴ����ֽ�����ע��������
	extern "C"
		unsigned long HDS_UDP_EXPORT_API get_uploaded_byte_count();

	extern "C"
	unsigned short HDS_UDP_EXPORT_API get_udp_port();
}


#endif