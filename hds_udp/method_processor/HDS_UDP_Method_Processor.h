#ifndef _HDS_METHOD_PROCESSOR_H_
#define _HDS_METHOD_PROCESSOR_H_

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "../common/HDS_Thread.h"
#include "../common/HDS_Unistd.h"
#include "../config/HDS_UDP_Kernel_Config.h"
#include "../connection/HDS_UDP_Connection.h"


#ifdef _SUPPORT_ACE_
#include <ace/Message_Block.h>
#include <ace/INET_Addr.h>
#endif

#ifdef _SUPPORT_ACE_
class __declspec(dllexport) I_UDP_Processor
{
public:
	/*
	注意！！！
	在可靠传输中，如果返回值是NO_DETELE_AFTER_PROCESS（即 return NO_DETELE_AFTER_PROCESS;），HDS_UDP内部才不会把ace_block中的数据清除。
	*/
	virtual int process(ACE_Message_Block *block, ACE_INET_Addr& remoteAddr) = 0;
};
typedef  I_UDP_Processor* PI_UDP_Processor;
#else
typedef int (HDS_UDP_API* I_UDP_Processor)(HDS_UDP_Result_Process_Data * parm);
typedef I_UDP_Processor  PI_UDP_Processor; 
#endif

namespace HDS_UDP
{
	namespace HDS_UDP_Method_Processor
	{
		int init_processor(const int& thread_count);

		int registe_method(MsgType msgType, const PI_UDP_Processor proc);

		void push_msg(const HDS_UDP_Result_Process_Data & process_data);

#ifdef _SUPPORT_ACE_
		int query_separate_recv_buf_AceBlock(ACE_Message_Block* block);
#else
		int query_separate_recv_buf(HDS_UDP_Result_Process_Data*);
#endif

		int fini_porcessor();
	}
}

#endif