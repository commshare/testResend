#ifndef _HDS_UDP_KERNEL_CONFIG_H_
#define _HDS_UDP_KERNEL_CONFIG_H_

#define HDS_UDP_DEBUG

//�Ƿ�����ACE��֧��
#define _SUPPORT_ACE_
#ifdef _SUPPORT_ACE_

#ifdef _DEBUG
#pragma comment(lib,"aced.lib")
#else
#pragma comment(lib,"ace.lib")
#endif

#endif

#ifdef WIN32
#pragma comment(lib,"ws2_32.lib")
#endif

//�Ƿ�֧��HDS_UDP 2.x��API,֧��HDS_UDP 2.x ��ʧȥ3.0�汾��һЩ��չ
#define _SUPPORT_HDS_UDP_2_

#define HDS_UDP_DEFAULT_MAX_WINDOWS_SIZE ((unsigned long)-1)

//�������İ���С
#define MAX_UDP_PACKET_SIZE 1472
#define HDS_UDP_PACKET_MAX_HEADER_LENGTH 20
#define HDS_UDP_PACKET_MAX_DATA_LENGTH (MAX_UDP_PACKET_SIZE-HDS_UDP_PACKET_MAX_HEADER_LENGTH)


//��ʱsocket ������
#define HDS_UDP_SOCKET_RECV_BUF_SIZE 10*1024*1024 //10MB
#define HDS_UDP_SOCKET_SEND_BUF_SIZE 512*1024 //512KB

//����udp�ķ����ٶ�
#define _LIMIT_UDP_SEND_SPEED_
#ifdef linux
#define LIMIT_500_US_SEND_SEPPD MAX_UDP_PACKET_SIZE*4  //byte
#endif



 //�������������Ŀ��������power of 2
#define HDS_UDP_CONNECTION_MARK_BITS 20
#define HDS_UDP_MAX_CONNECTION_COUNT (1<<HDS_UDP_CONNECTION_MARK_BITS)
#define HDS_UDP_CONNECTION_OFFSET_SHIRT ((1<<HDS_UDP_CONNECTION_MARK_BITS)-1)


//////////////////////////////////��Ϣ����////////////////////////////////////////
/*���ļ�����Э��*/
#define HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE  115
#define HDS_UDP_TRANS_SIMPLE_FILE_DATA       116
#define HDS_UDP_TRANS_SIMPLE_FILE_ACK          117
#define HDS_UDP_TRANS_SIMPLE_FILE_REJECTED  118
#define HDS_UDP_TRANS_SIMPLE_FILE_REQUIRE_DUPLICATE 119
/*�����ļ�����Э��*/
#define HDS_UDP_TRANS_COMPLEX_FILE_REQUIRE 100
#define HDS_UDP_TRANS_COMPLEX_FILE_DATA      101
#define HDS_UDP_TRANS_COMPLEX_FILE_ACK        102
#define HDS_UDP_TRANS_COMPLEX_FILE_REJECTED 103
#define HDS_UDP_TRANS_COMPLEX_FILE_SACK       104
#define HDS_UDP_TRANS_COMPLEX_FILE_REQUIRE_DUPLICATE 105
#define HDS_UDP_TRANS_RESET 106
//////////////////////////////////////////////////////////////////////////


////////////////////////////////�����ļ�����Э��ѡ��//////////////////////////////////////////
#define HDS_UDP_DEFAULT_SSTHRESH_SIZE 64*HDS_UDP_PACKET_MAX_DATA_LENGTH
#define HDS_UDP_CONNECTION_RECV_BITMAP_SIZE 384 //��������ѡ�����ش���bitmap�Ĵ�С
#define _USE_COMPLEX_FILE_WINDOW_INCREASE_FACTOR_
//////////////////////////////////////////////////////////////////////////


#define SMALL_FILE_MAX_SIZE  16*HDS_UDP_PACKET_MAX_DATA_LENGTH


////////////////////////////////////timerѡ��//////////////////////////////////////
#define DEFAULT_RTO 2000 //��ʼ��RTOֵ����λms
#define HDS_UDP_CONNECTION_DEFAULT_RTO 3000
#define HDS_UDP_CONNECTION_MAX_SUSPEND_TIME 1000
#define HDS_UDP_RESET_REACTION_TIME 2000
#define HDS_UDP_RECV_ACK_TIMER 200
#define HDS_UDP_RECVER_SUCC_TIMER 100
#define HDS_UDP_CONNECTION_RECV_CHECK_DEAD_TIME 75000
#define HDS_UDP_CONNECTION_SEND_TRY_TIME 7
#define HDS_UDP_CONNECTION_MIN_RTO 1000L
#define HDS_UDP_CONNECTION_MAX_RTO 8000L
//////////////////////////////////////////////////////////////////////////


/////////////////////////////////TCPѡ��//////////////////////////////////////////////
#define HDS_TCP_IDEL_CLOSE_TIME 900000 //90000 //tcp�ڿ���90000s֮��ͻ��Զ��ر�
#define HDS_TCP_QUERY_MSG_LENGTH 1024 //1024
#define HDS_TCP_CONNECTION_RECV_BUF_LENGTH 65535
////////////////////////////////////////////////////////////////////////////////////////

#endif
