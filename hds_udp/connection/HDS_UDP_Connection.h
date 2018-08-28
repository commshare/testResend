 #ifndef  _HDS_UDP_Connction_ID_H_
#define _HDS_UDP_Connction_ID_H_

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#endif

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../common/atomic_t.h"
#include "../common/HDS_Unistd.h"
#include "../common/HDS_Socket_Utils.h"
#include "../config/HDS_UDP_Kernel_Config.h"
#include "../kernel/HDS_UDP_Kernel_Data_Filter.h"
#include "../common/HDS_WriteCDR.h"

#ifdef _SUPPORT_ACE_
#include <ace/Message_Block.h>
#include <ace/INET_Addr.h>
#endif


typedef int MsgType;

namespace HDS_UDP
{
	enum SENDED_DEL_TYPE
	{
		DELETE_AFTER_SEND = 1,
		NO_DELETE_AFTER_SEND =0
	};
}

typedef HDS_UDP::SENDED_DEL_TYPE HDS_UDP_SENDED_DEL_TYPE;

namespace HDS_UDP
{
	enum TRANS_RESULT_CODE
	{
		TRANS_SUCCESSFULLY =1,
		TRANS_TIME_OUT =2,
		TRANS_REJECTED=3,
		TRANS_NAT_FAILED =4,
		TRANS_UNKOWN=5,
		TRANS_GUID_CONFLICT=6,
		TRANS_FORCE_STOP=7,
		TRANS_KERNEL_NO_SPACE=8,
		TRANS_RESET=9,
		TRANS_CONNECT_ERR=10
	};
	typedef TRANS_RESULT_CODE SENDED_RESULT_CODE;
}
typedef HDS_UDP::TRANS_RESULT_CODE HDS_UDP_TRANS_RESULT_CODE;

namespace HDS_UDP
{
	enum Recv_Buf_Type
	{
		POOL_SPACE=1,
		USER_SPACE_DEL=2,
		USER_SPACE_NO_DEL=3,
		MALLOC_SPACE_DEL=4,
		MALLOC_SPACE_NO_DEL=5
	};
}
typedef HDS_UDP::Recv_Buf_Type HDS_UDP_Recv_Buf_Type;

namespace HDS_UDP
{
	enum Verification_Recv_Type
	{
		Verification_Recv_Type_No_Recv=1,
		Verification_Recv_Type_Recv=2
	};
}
typedef HDS_UDP::Verification_Recv_Type HDS_UDP_Verification_Recv_Type;

namespace HDS_UDP
{
	typedef struct
	{
		sockaddr remote_addr;
		socklen_t remote_addr_length;
		int          data_length;
		HDS_UDP_Recv_Buf_Type buf_type;
		char   *   data;
	}Result_Process_Data;
}
typedef HDS_UDP::Result_Process_Data HDS_UDP_Result_Process_Data;

#ifdef _SUPPORT_HDS_UDP_2_
typedef   void    (CALLBACK *HDS_UDP_Sender_Handler)( char* block,unsigned int length, sockaddr_in & remoteAddr, HDS_UDP_TRANS_RESULT_CODE code);
#else
typedef   void    (CALLBACK *HDS_UDP_Sender_Handler)( char* block,unsigned int length, 
													 sockaddr* remote_addr, socklen_t remote_addr_length,
													 HDS_UDP_TRANS_RESULT_CODE code,HDS_UDP_SENDED_DEL_TYPE &type);
#endif

namespace HDS_UDP
{
	class  HDS_UDP_EXPORT message_node
	{
	public:
		message_node():next(NULL){}
		message_node(char * sdata , unsigned int slength, HDS_UDP_SENDED_DEL_TYPE dtype, HDS_UDP_Sender_Handler sHandle):
		source_data(sdata),source_data_length(slength),del_type(dtype),send_handle(sHandle),next(NULL)
		{}
		HDS_UDP_Sender_Handler send_handle;
		message_node* next;
		HDS_UDP_SENDED_DEL_TYPE del_type;
		char * source_data;
		unsigned int source_data_length;
	};
}
typedef HDS_UDP::message_node HDS_UDP_message_node;


namespace HDS_UDP
{
	struct  HDS_UDP_EXPORT CONNECTION_INFO
	{
		long complete_size; //byte
		long total_size;
		HDS_UDP_TRANS_RESULT_CODE trans_result_code;
		sockaddr remote_addr;
		socklen_t remote_addr_length;
	};
}
typedef HDS_UDP::CONNECTION_INFO HDS_UDP_CONNECTION_INFO;

namespace HDS_UDP
{
	class Connection_ID
	{
	public:
		union
		{
			long long int _ID64;
			unsigned int _ID32[2];
			unsigned char _ID8[8];
		}_CID;

	public:
		Connection_ID()
		{
			this->_CID._ID64=0;
		}

		int create_connectionID(const void* connection_addr);

		void create_connectionID_label();

		void suspend_connection_ID();

		//回收ID
		void release_connection_ID();

		/*重载== 看两者是否相等*/
		inline bool operator==(const Connection_ID &op)const
		{
			return this->_CID._ID64 == op._CID._ID64;
		}

		/*重载<*/
		inline bool operator < (const Connection_ID & op ) const
		{
			return this->_CID._ID64 < op._CID._ID64;
		}

		inline bool operator > (const Connection_ID & op ) const
		{
			return this->_CID._ID64 > op._CID._ID64;
		}

		inline bool is_not_setted() const
		{
			return this->_CID._ID64 ==0;
		}

		inline void set_zero()
		{
			this->_CID._ID64=0;
		}
	};

	typedef Connection_ID FILE_ID;
}
typedef HDS_UDP::Connection_ID HDS_UDP_Connection_ID;


namespace HDS_UDP
{
	enum Verification_Recv_Buf_Type
	{
		Verification_Recv_Buf_Type_Manual_Manger=1,
		Verification_Recv_Buf_Type_Auto_Delete=2
	};
}
typedef HDS_UDP::Verification_Recv_Buf_Type HDS_UDP_Verification_Recv_Buf_Type;

//数据认证
typedef void (CALLBACK *HDS_UDP_Recv_Failed_Func)(HDS_UDP_Result_Process_Data *data,
										 HDS_UDP_Connection_ID *conID,HDS_UDP_TRANS_RESULT_CODE code);
typedef struct 
{
	HDS_UDP_Verification_Recv_Type _recv_type;
	HDS_UDP_Verification_Recv_Buf_Type _recv_buf_type;
	char* _recv_buf;  //用户定义的接收区，为0则使用系统自定义缓冲区
	HDS_UDP_Recv_Failed_Func _recv_failed_func;
}HDS_UDP_Recv_Verification_Attribute;

#define init_HDS_UDP_Recv_Verification_Attribute(atrr) do \
{\
	atrr._recv_type = HDS_UDP::Verification_Recv_Type_Recv;\
	atrr._recv_buf_type = HDS_UDP::Verification_Recv_Buf_Type_Auto_Delete;\
	atrr._recv_buf = NULL;\
	atrr._recv_failed_func = NULL;\
} while (0);


typedef HDS_UDP_Recv_Verification_Attribute (CALLBACK *HDS_UDP_Recv_Verification_Func)(char* recv_data,
															  int recv_data_length,int total_data_length,HDS_UDP_Connection_ID *conID,
															  sockaddr* remote_addr, socklen_t remote_addr_length);
namespace HDS_UDP
{
	enum Connection_Timer_Type
	{
		Sender_Keeping_Timer=1,
		Recver_Check_Dead_Timer=2,
		Connection_Suspend_Timer=3,
		Force_Reset_Timer=4,
		Recver_ACK_Timer=5,
		Recver_Succ_Timer=6
	};

	typedef struct 
	{
		long running_count;
		Connection_Timer_Type timeout_type;
		int connection_offset;
		void* connection_addr;
		void* extend_data;
	}_timer_parm_;

	//考虑到多态的性能消耗，所以不使用多态
	struct connection
	{
		Connection_ID _connection_ID;
		atomic_t<long> _running_count;
		HDS_UDP_TRANS_RESULT_CODE _trans_result_code;
		//*****************************************
		//status 的含义 
		//0 bit 传输是否成功
		//1 bit 数据清理位
		//2 bit 强制停止位
		//3 bit 标记是接收端还是发送端 
#define _CNT_STATUS_TRANS_SUC_BIT_OFFSET_ 0
#define _CNT_STATUS_BUF_CLEAN_BIT_OFFSET_  1
#define _CNT_STATUS_FORCE_STOP 2
#define _CNT_CONNECTION_TYPE 3
#define _CNT_BASE_CONNECTION_END _CNT_CONNECTION_TYPE

#define get_connection_offset(con) \
	(con->_connection_ID._CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT)

#define  set_connection_trans_succ(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_TRANS_SUC_BIT_OFFSET_))

#define test_connection_trans_succ(con) \
	(con->_status.BitTest(_CNT_STATUS_TRANS_SUC_BIT_OFFSET_))

#define set_connection_trans_suspend(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_BUF_CLEAN_BIT_OFFSET_))

#define set_connection_force_stop(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_FORCE_STOP))

#define test_connection_force_stop(con) \
	(con->_status.BitTest(_CNT_STATUS_FORCE_STOP))

#define set_connection_type_reader(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_CONNECTION_TYPE))

#define  test_connection_type_reader(con) \
	(con->_status.BitTest(_CNT_CONNECTION_TYPE)==0)

#define set_connection_type_writer(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_CONNECTION_TYPE))

#define test_connection_type_writer(con) \
	(con->_status.BitTest(_CNT_CONNECTION_TYPE)!=0)

		atomic_t<long> _status;
		//*****************************************
		
		char * _transmit_buf;
		unsigned int      _transmit_buf_length;
		sockaddr _remote_addr;
		socklen_t  _remote_addr_length;

		void stop_connection(const HDS_UDP_TRANS_RESULT_CODE code);

		inline void set_remote_addr(sockaddr* addr ,socklen_t addr_length)
		{
			memcpy(&this->_remote_addr,addr,addr_length);
			this->_remote_addr_length = addr_length;
		}

		atomic_t<unsigned long> _left_window_cursor;
#define set_connection_left_window_cursor(con,seq,res) \
	do{\
	long base_seq=seq;\
	long lseq = seq;long olseq = seq;\
	do{lseq = olseq;olseq =con->_left_window_cursor.AtomicExchange(lseq);}while(olseq > lseq);\
	res = lseq<=base_seq?0:-1;\
	}while(0)

		typedef void (*connection_free_memory)(connection* con);
		typedef void (*conneciton_do_time_out)(connection* con,Connection_Timer_Type timer_type,long running_count,void* extend_data); 
		typedef void (*conneciton_do_network_data)(connection *con,const MsgType msgtype,unsigned int seq, int send_time,char* recv_data, int recv_data_length,Kernel_Recv_Data* data);
		typedef void (*increase_connection_type_count)();
		typedef void (*decrese_conneciton_type_count)();
		typedef unsigned long (*get_connection_type_count)();
		conneciton_do_time_out _do_time_out;
		conneciton_do_network_data _do_network_data;
		connection_free_memory _connection_free_memory;
		increase_connection_type_count _increase_connection_type_count;
		decrese_conneciton_type_count _decrease_conneciton_type_count;
		get_connection_type_count _get_connection_type_count;

        static connection* get_connection(const int offset);

        static int init();

		static int fini(){return 0;}

		static void filter_data(Kernel_Recv_Data* rdata);

		//如果block_length ==0 则发送的是message_node
		static int start_sender(message_node* node,char * block , unsigned int block_length ,
			sockaddr* remote_addr,socklen_t remote_addr_length,Connection_ID* conID,
			HDS_UDP_SENDED_DEL_TYPE del_type,HDS_UDP_Sender_Handler handler,
			unsigned long defaultSSTHRESH,
			unsigned long maxWindowSize);

#ifdef _SUPPORT_ACE_
		static int start_sender_ace(ACE_Message_Block* block,ACE_INET_Addr& remoteAddr,Connection_ID* conID,
			HDS_UDP_SENDED_DEL_TYPE del_type,HDS_UDP_Sender_Handler handler,
			unsigned long defaultSSTHRESH,
			unsigned long maxWindowSize);
#endif

		static int start_link_send(message_node * node,
			sockaddr* remote_addr,socklen_t remote_addr_length,Connection_ID* conID,
			unsigned long defaultSSTHRESH,
			unsigned long maxWindowSize);

		static void start_recver(MsgType type, Kernel_Recv_Data* data);

		void free_connetcion();

		void add_to_timer_queue(long DueTime,Connection_Timer_Type timeout_type,void* extend_data);

		static void connection_do_time_out(_timer_parm_ *parm);

		static void connection_do_network_data(const MsgType msgtype,Kernel_Recv_Data* rdata);

		static void connection_do_require_duplicate(const MsgType msgtype,Kernel_Recv_Data* rdata);

		static void connection_do_connection_reset(const MsgType msgtype,Kernel_Recv_Data* rdata);

		static int connection_do_connection_stop(Connection_ID * conID);

		static unsigned long get_simple_connection_reader_num();

		static unsigned long get_simple_connection_writer_num();

		static unsigned long get_complex_connection_reader_num();

		static unsigned long get_complex_conenction_writer_num();

		static int get_connection_info(CONNECTION_INFO* info, Connection_ID* con_ID);

		///////////////////////////////////////////////////////////////////////////////////
		static void set_recv_verification_func(HDS_UDP_Recv_Verification_Func func);
		/////////////////////////////////////////////////////////////////////////////////////
	};

#ifdef WIN32
#define connection_clock(timev) \
	do{\
	timev = clock();\
	}while(0)
#else
#define connection_clock(timev) \
	do{\
	timeval dif;\
	gettimeofday(&dif,NULL);\
	uint64_t uv = dif.tv_sec*1000+dif.tv_usec/1000;\
	timev = (int)(uv & 0xffffffff);\
	}while(0)
#endif

#define CONNECTION_FILL_HEADER(msgtype,conID,send_time,dt_note,cdr) \
	do{cdr.write_long(msgtype);\
	cdr.write_ulong(conID._CID._ID32[1]);\
	cdr.write_ulong(conID._CID._ID32[0]);\
	cdr.write_long(send_time);\
	cdr.write_ulong(dt_note);}while(0)

#define CONNECTION_UNFILL_HEADER(cdr,conID,send_time,dt_note,recv_data,recv_data_length) \
	do{\
	cdr.skip_long();\
	cdr.read_ulong(conID._CID._ID32[0]);\
	cdr.read_ulong(conID._CID._ID32[1]);\
	cdr.read_long(send_time);\
	cdr.read_ulong(dt_note);\
	recv_data = cdr.get_current_pos();\
	recv_data_length = rdata->data_length-cdr.length();\
	}while(0)

#define CONNECTION_GET_CONNECTID_OFFSET_FROM_HEAD(rdata) \
	(((int)*(rdata->data+4))&HDS_UDP_CONNECTION_OFFSET_SHIRT)

enum Ack_Type
{
	ACK=0,
	SACK=1
};

	struct Connection_Writer:public connection
	{
		//status 标志的意思
		//3 bit 是否已经在sender队列中
		//4 bit 标记是否需要将connection加入回sender队列
		//5 bit 标记是否在更新RTO
		//6 bit 是否发送过Require，如果发送过则需要使用requrie_duplicate
		//7 bit 标记是否有sack出现
		//8 bit 是否存在定时器超时
		//9 bit 标记是否正在处理ack或者sack信息，有可能多线程调用同时进入ack处理，这时候后到的ack只好丢弃。这种情况发送的几率很低。
		//10 bit 标识是否仍然处于sack阶段,此阶段无法发送新的报文
		//11 bit 标识是否已经重传了丢失的包，并可以恢复到原来current_cursor 
#define _CNT_STATUS_IN_SENDER_BIT_OFFSET_ (_CNT_BASE_CONNECTION_END+1)
#define _CNT_STATUS_REBACK_SENDER_BIT_OFFSET (_CNT_BASE_CONNECTION_END+2)
#define _CNT_STATUS_REFLESH_RTO_BIT_OFFSET (_CNT_BASE_CONNECTION_END+3)
#define _CNT_STATUS_HAS_SEND_REQUIE_BIT_OFFSET (_CNT_BASE_CONNECTION_END+4)
#define _CNT_STATUS_HAS_SACK_BIT_OFFSET (_CNT_BASE_CONNECTION_END+5)
#define _CNT_STATUS_HAS_TIME_OUT_BIT_OFFSET (_CNT_BASE_CONNECTION_END+6)
#define _CNT_STATUS_DOING_ACK_BIT_OFFSET (_CNT_BASE_CONNECTION_END+7)
#define _CNT_STATUS_STILL_DOING_SACK_BIT_OFFSET (_CNT_BASE_CONNECTION_END+8)
#define _CNT_STATUS_SACK_HAS_NEW_ACK_OFFSET (_CNT_BASE_CONNECTION_END+9)

#define set_connection_in_sender(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_IN_SENDER_BIT_OFFSET_))
#define reset_connection_in_sender(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_STATUS_IN_SENDER_BIT_OFFSET_))
#define set_connection_reback_sender(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_REBACK_SENDER_BIT_OFFSET))
#define reset_connection_reback_sender(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_STATUS_REBACK_SENDER_BIT_OFFSET))
#define lock_connection_status_reflesh_rto_mutex(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_REFLESH_RTO_BIT_OFFSET))
#define unlock_connection_status_reflesh_rto_mutex(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_STATUS_REFLESH_RTO_BIT_OFFSET))
#define set_connection_status_has_send_require(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_HAS_SEND_REQUIE_BIT_OFFSET))
#define set_connection_status_has_sack(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_HAS_SACK_BIT_OFFSET))
#define reset_connection_status_has_sack(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_STATUS_HAS_SACK_BIT_OFFSET))
#define set_connection_status_has_time_out(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_HAS_TIME_OUT_BIT_OFFSET))
#define reset_connection_status_has_time_out(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_STATUS_HAS_TIME_OUT_BIT_OFFSET))
#define lock_connection_status_ack_mutex(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_DOING_ACK_BIT_OFFSET))
#define unlock_connection_status_ack_mutex(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_STATUS_DOING_ACK_BIT_OFFSET))
#define set_connection_status_in_process_sack(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_STILL_DOING_SACK_BIT_OFFSET))
#define reset_connection_status_in_process_sack(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_STATUS_STILL_DOING_SACK_BIT_OFFSET))
#define test_connection_status_in_process_sack(con) \
	(con->_status.BitTest(_CNT_STATUS_STILL_DOING_SACK_BIT_OFFSET))
#define set_sack_has_new_ack(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_SACK_HAS_NEW_ACK_OFFSET))
#define reset_sack_has_new_ack(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_STATUS_SACK_HAS_NEW_ACK_OFFSET))



//////////////////////*****RTO*****////////////////////////////////
		volatile long _SRTT;
		volatile long _RTTVAR;
		atomic_t<long> _RTO;

#define	rto_diff(x,y)  x>y?(x-y):(y-x)

/*
基本算法
要计算当前的RTO，TCP发送方需要维护两个状态变量，SRTT （平滑环回时间）和RTTVAR（环回时间变量）。另外，我们假设一个时钟间隔G秒。
计算SRTT、RTTVAR和RTO的法则如下：
(1) 在对一个收发双方之间所发出的一个段完成回环时间（RTT）测量之前，发送方应该将RTO设置为3秒。
注意到有一些设施可能采用一种“心跳式”定时器，它能够产生一个介于2.5秒和3秒之间的值。因此，在该定时器绝不会在短于2.5秒的时间内超时的情况下，作为一个较低的2.5秒的步进也是可以接受的，使用间隔为G的心跳式定时器的设施，定时器的值不低于2.5 + G秒。
(2) 当完成第一个RTT测量R时，宿主机必须设置
SRTT <— R
RTTVAR <—R/2
RTO <—SRTT + max (G, K*RTTVAR)
其中K = 4。
(3) 当完成一个并发的RTT测量R'时，宿主机必须设置
RTTVAR <—(1 - beta) * RTTVAR + beta * |SRTT - R'|
SRTT <— (1 - alpha) * SRTT + alpha * R'
用来更新RTTVAR的SRTT的值，就是那个使用第二次分配来更新SRTT之前的SRTT值本身。就是说，更新RTTVAR和SRTT必须按照上述的顺序进行计算。
上述计算应该使用alpha=1/8和beta=1/4进行计算。
在计算之后，宿主机必须更新RTO <— SRTT + max (G, K*RTTVAR)。
(4) 一旦RTO计算好，如果它小于1秒钟，则RTO应该补充到1秒。
*/
#define  set_connection_RTO(con,send_time) \
	do{\
	if(lock_connection_status_reflesh_rto_mutex(con)==0){\
	int con_clock;\
	connection_clock(con_clock);\
	long RTT = con_clock-send_time;\
	if(RTT>0){\
	if(con->_SRTT==0){con->_SRTT=RTT;con->_RTTVAR=RTT>>1;}\
	else{con->_RTTVAR = (3*con->_RTTVAR +rto_diff(con->_SRTT,RTT))>>2;con->_SRTT=(7*con->_SRTT+RTT)>>3;}\
	con->_RTO = con->_SRTT+(con->_RTTVAR<<2);\
	if(con->_RTO.value() < HDS_UDP_CONNECTION_MIN_RTO){con->_RTO=HDS_UDP_CONNECTION_MIN_RTO;}\
	if(con->_RTO.value() > HDS_UDP_CONNECTION_MAX_RTO){con->_RTO = HDS_UDP_CONNECTION_MAX_RTO;}\
	}\
	unlock_connection_status_reflesh_rto_mutex(con);\
	}}while(0)

#define set_connection_RTO_Double(con) \
	do{\
	if(lock_connection_status_reflesh_rto_mutex(con)==0){\
	con->_RTO= con->_RTO.value()<<1;\
	if(con->_RTO.value()>HDS_UDP_CONNECTION_MAX_RTO)\
		{con->_RTO=HDS_UDP_CONNECTION_MAX_RTO;}\
	unlock_connection_status_reflesh_rto_mutex(con);\
	}}while(0)
////////////////////////////////////////////////////////////////////
		/*
		inline void test_set_connection_RTO(long send_time)
		{
				if(lock_connection_status_reflesh_rto_mutex(this)==0)
				{
					int con_clock;
					connection_clock(con_clock);
					long RTT = con_clock-send_time;
					printf("RTT %d\n",RTT);
					if(RTT>0){
						if(this->_SRTT==0)
						{
							this->_SRTT=send_time;
							this->_RTTVAR=send_time>>1;
						}
						else
						{
							this->_RTTVAR = (3*this->_RTTVAR +rto_diff(this->_SRTT,RTT))>>2;
							this->_SRTT=(7*this->_SRTT+RTT)>>3;
						}
						this->_RTO = this->_SRTT+(this->_RTTVAR<<2);
						if(this->_RTO.value() < HDS_UDP_CONNECTION_MIN_RTO){this->_RTO=HDS_UDP_CONNECTION_MIN_RTO;}
						if(this->_RTO.value() > HDS_UDP_CONNECTION_MAX_RTO){this->_RTO = HDS_UDP_CONNECTION_MAX_RTO;}
					}
					unlock_connection_status_reflesh_rto_mutex(this);
				}
		}
		*/

		HDS_UDP_Sender_Handler _sended_handle;

		void writer_do_connection_sended_handle_and_clean_data();
		
		typedef void (*conneciton_do_send)(connection* con,char* sender_buf);
		conneciton_do_send      _do_send;
		HDS_UDP_SENDED_DEL_TYPE  _del_type;
		//传输的尝试次数
		atomic_t<long> _try_time;

#ifdef _SUPPORT_ACE_
		ACE_Message_Block* _ace_transmit_block;
#endif

		message_node * _trans_message_node;

		void add_to_sender_queue();
		static void connection_do_sender(const int offset,char* sender_buf);
	};

/*
	static void test_CONNECTION_FILL_DATA(HDS_WriteCDR &cdr , unsigned int seq, Connection_Writer * writer)
	{
		unsigned int copy_size = writer->_transmit_buf_length - seq;
		if(copy_size>HDS_UDP_PACKET_MAX_DATA_LENGTH){copy_size=HDS_UDP_PACKET_MAX_DATA_LENGTH;}
		if(writer->_transmit_buf){cdr.write_char_array(writer->_transmit_buf+seq,copy_size);}
		else{
			char* start_pos= NULL;
			long offset = seq;
			message_node* msg_node = writer->_trans_message_node;
			while(msg_node){
				offset = offset - msg_node->source_data_length;
				if(offset<0){offset = (long)(msg_node->source_data_length)+offset;start_pos = msg_node->source_data+offset;break;}
				else if(offset ==0){start_pos= msg_node->next->source_data;msg_node = msg_node->next;break;}
				else{msg_node= msg_node->next;}
			}
			unsigned int tempsz=0;
			while(msg_node){
				tempsz = msg_node->source_data_length-(start_pos - msg_node->source_data);
				if(tempsz>=copy_size)
				{
					cdr.write_char_array(start_pos,copy_size);
					break;
				}
				else
				{
					cdr.write_char_array(start_pos,tempsz);
					copy_size -= tempsz;
					msg_node = msg_node->next;
					start_pos = msg_node->source_data;
				}
			}
		}
	}
*/

#define CONNECTION_FILL_DATA(cdr,seq,writer) \
	do{\
	unsigned int copy_size = writer->_transmit_buf_length - seq;\
	if(copy_size>HDS_UDP_PACKET_MAX_DATA_LENGTH){copy_size=HDS_UDP_PACKET_MAX_DATA_LENGTH;}\
	if(writer->_transmit_buf){cdr.write_char_array(writer->_transmit_buf+seq,copy_size);}\
		else{\
		char* start_pos= NULL;\
		long offset = seq;\
		message_node* msg_node = writer->_trans_message_node;\
		while(msg_node){\
		offset = offset - msg_node->source_data_length;\
		if(offset<0){offset = (long)(msg_node->source_data_length)+offset;start_pos = msg_node->source_data+offset;break;}\
		else if(offset ==0){start_pos= msg_node->next->source_data;msg_node = msg_node->next;break;}\
		else{msg_node= msg_node->next;}\
		}\
		unsigned int tempsz=0;\
		while(msg_node){\
		tempsz = msg_node->source_data_length-(start_pos - msg_node->source_data);\
		if(tempsz>=copy_size){cdr.write_char_array(start_pos,copy_size);break;}\
		else{cdr.write_char_array(start_pos,tempsz);copy_size -= tempsz;msg_node = msg_node->next;start_pos = msg_node->source_data;}\
		}\
		}\
	}while(0)

	struct Connection_Reader:public connection
	{
		//status 标志的意思
		//2bit表示是否在recv_wait_ack队列中
		//3bit表示是否已经触发接收成功定时器
		//4bit表示是否出现SACK
#define _CNT_STATUS_IN_RECV_WAIT_ACK_QUEUE_BIT_OFFSET (_CNT_BASE_CONNECTION_END+1)
#define _CNT_STATUS_SET_RECV_SUCC_TIMER_BIT_OFFSET (_CNT_BASE_CONNECTION_END+2)
#define _CNT_STATUS_READER_SACK_BIT_OFFSET (_CNT_BASE_CONNECTION_END+3)

#define set_reader_connection_status_recv_wait_ack_queue(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_IN_RECV_WAIT_ACK_QUEUE_BIT_OFFSET))
#define reset_reader_connection_status_recv_wait_ack_queue(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_STATUS_IN_RECV_WAIT_ACK_QUEUE_BIT_OFFSET))
#define set_reader_connection_status_recv_succ_timer(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_SET_RECV_SUCC_TIMER_BIT_OFFSET))
#define set_reader_connection_sack(con) \
	(con->_status.AtomicBitTestAndSet(_CNT_STATUS_READER_SACK_BIT_OFFSET))
#define reset_reader_connection_sack(con) \
	(con->_status.AtomicBitTestAndReset(_CNT_STATUS_READER_SACK_BIT_OFFSET))
#define test_reader_connection_sack(con) \
	(con->_status.BitTest(_CNT_STATUS_READER_SACK_BIT_OFFSET)==1)

		//接收缓冲区的类型
		volatile unsigned int _require_duplicate_label;
		HDS_UDP_Recv_Buf_Type _recv_buf_type;
		HDS_UDP_Recv_Failed_Func _recv_failed_handle;

		void reader_do_connection_sended_handle_and_clean_data();

		static void set_complex_file_need_verification();
		
		static void reset_complex_file_need_verification();
	};
}
#endif