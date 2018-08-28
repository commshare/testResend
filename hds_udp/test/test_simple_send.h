#include "../common/atomic_t.h"
#include "../common/AutoLock.h"
#include "../common/HDS_Event.h"
#include "../common/HDS_Timer_Wheel.h"
#include "../common/HDS_Unistd.h"
#include "../common/Atomic_Bitmap.h"
#include "../common/HP_Map.h"
#include "../common/HP_Set.h"
#include "../common/HDS_Blocking_Queue.h"
#include "../kernel/HDS_UDP_Kernel.h"
#include "../common/HDS_Unistd.h"
#include "../common/HDS_Socket_Utils.h"
#include "../common/HDS_Error.h"
#include "../buffer/HDS_Buffer_Pool.h"
#include "../connection/HDS_UDP_Connection.h"
#include "../connection/HDS_UDP_Connection_Ack_Info_Collector.h"
#include "../connection/HDS_UDP_Simple_Connection.h"
#include "../include/HDS_UDP.h"
#include "../method_processor/HDS_UDP_Method_Processor.h"
#include <stdio.h>
#include <list>
#include <fstream>
#include <string.h>

atomic_t<long> _recv_countb=0;
using namespace HDS_UDP;

#ifdef _SUPPORT_ACE_
class ss_processor2:public I_UDP_Processor
{
	int process(ACE_Message_Block *block, ACE_INET_Addr& remoteAddr)
	{
		char * buf = block->rd_ptr();
		int res = query_separate_recv_buf_AceBlock(block);
		printf("%s,%d\n",buf+4,_recv_count++);
		return 0;
	}
};
#else
int HDS_UDP_API simpleProcessor2(HDS_UDP_Result_Process_Data * parm)
{
	int res = query_separate_recv_buf(parm);
	printf("%s,%d\n",&parm->data[4],_recv_count++);
	return 0;
}
#endif

void test_simple_send_size_16_1()
{
#ifdef WIN32
	if(init_HDS_UDP(NULL,5150,4,4)==-1)
		return;
#else
	if(init_HDS_UDP("192.168.0.200",5150,4,4)==-1)
		return;
#endif

	sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_port=htons(5150);
#ifdef linux
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
#else
	addr.sin_addr.s_addr=inet_addr("127.0.0.1");
#endif
	char data[12]="hello world";
	char sdata[16];
	int * d = (int*)sdata;
	*d = 100;
	memcpy((char*)&sdata[4],(char*)&data,12);

#ifdef _SUPPORT_ACE_
	regeditProcessor(100,new ss_processor2);
#else
	regeditProcessor(100, simpleProcessor2);
#endif

	CSend(sdata,16,addr,NO_DELETE_AFTER_SEND);
	Sleep(10000000);
}

int HDS_UDP_API simpleProcessor_write_file(HDS_UDP_Result_Process_Data * parm)
{
	char name[256];
#ifdef WIN32
	sprintf_s(name,256,"F:\\%s%d.mp3","movie",_recv_countb++);
#else
	sprintf(name,"F:\\%s%d.mp3","movie",_recv_countb++);
#endif
	
	std::ofstream onf(name,std::ios::binary);
	if(onf)
	{
		onf.write(parm->data+4,parm->data_length-4);
		onf.close();
	}
	return 0;
}

#ifdef _SUPPORT_ACE_
class ss_processor_write_file: public I_UDP_Processor
{
	int process(ACE_Message_Block *block, ACE_INET_Addr& remoteAddr)
	{
		char name[256];
#ifdef WIN32
		sprintf_s(name,256,"F:\\%s%d.mp3","movie",_recv_countb++);
#else
		sprintf(name,"F:\\%s%d.mp3","movie",_recv_countb++);
#endif

		std::ofstream onf(name,std::ios::binary);
		if(onf)
		{
			onf.write(block->rd_ptr()+4,block->length()-4);
			onf.close();
		}
		return 0;
	}
};
#endif

void test_simple_send_size_8M_1()
{
#ifdef WIN32
	if(init_HDS_UDP(NULL,5150,4,4)==-1)
		return;
#else
	if(init_HDS_UDP("222.201.139.146",5150,4,4)==-1)
		return;
#endif

	sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_port=htons(5150);
#ifdef linux
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
#else
	addr.sin_addr.s_addr=inet_addr("127.0.0.1");
#endif

#ifdef _SUPPORT_ACE_
	regeditProcessor(1000,new ss_processor_write_file);
#else
	regeditProcessor(1000, simpleProcessor_write_file);
#endif

	std::ifstream inf( "F:\\GTO.12.FeiXue.x264-OGG.mkv",std::ios::binary);
	if(inf)
	{
		inf.seekg(0,std::ios::end);
		unsigned int length= inf.tellg();
		char *myblock =new char[length+4];
		int x=1000;
		memcpy(myblock,&x,4);
		inf.seekg(0,std::ios::beg);
		inf.read(myblock+4,length);
		for(int i=0;i<1;i++)
		{
			CSend(myblock,length+4,addr);
		}
	}
	Sleep(100000000);
}


atomic_t <long > _succ_count=0;

#ifdef _SUPPORT_ACE_
class send_link_Processor_boundary_ace: public I_UDP_Processor
{
	int process(ACE_Message_Block *block, ACE_INET_Addr& remoteAddr)
	{
		printf("_count=%d\n",_succ_count++);
		char * x = block->rd_ptr()+4;
		for(unsigned int i=0;i<block->length()-4;i++)
		{
			if(x[i] != 'i')
			{
				printf("error");
			}
		}
		return 0;
	}
};
#else
int HDS_UDP_API send_link_Processor_boundary(HDS_UDP_Result_Process_Data * parm)
{
	printf("_count=%d\n",_succ_count++);
	char * x = parm->data+4;
	for(int i=0;i<(parm->data_length-4);i++)
	{
		if(x[i] != 'i')
		{
			printf("error");
		}
	}
	return 0;
}

#endif
int HDS_UDP_API simpleProcessor_boundary(HDS_UDP_Result_Process_Data * parm)
{
	//if(_succ_count++%100==0)
		printf("_count=%d\n",_succ_count++);

	for(int i=4;i<1491;i++)
	{
		if(parm->data[i]!='i')
		{
			int x =parm->data[i];
			char* ad = &parm->data[i];
			printf("error %d",x);
		}
	}
	return 0;
}

atomic_t <long > _send_failed_count=0;
void  CALLBACK send_boundary_Sender_Handler( char* block, unsigned int length, 
													 sockaddr* remote_addr, socklen_t remote_addr_length,
													 HDS_UDP_TRANS_RESULT_CODE code,HDS_UDP_SENDED_DEL_TYPE &type)
{
	if(TRANS_SUCCESSFULLY != code)
	{
		printf("_send_failed_count=%d\n",_send_failed_count++);
	}
	else
	{
		printf("_send_sucss_count=%d\n",_send_failed_count++);
	}
	int x=0;
}

atomic_t <long > _recv_failed_count=0;
void CALLBACK my_Recv_Failed_Func(HDS_UDP_Result_Process_Data *data,
										 HDS_UDP_Connection_ID *conID,HDS_UDP_TRANS_RESULT_CODE code)
{
	printf("_recv_failed_count=%d\n",_recv_failed_count++);
}

HDS_UDP_Connection_ID xxID;

atomic_t<long> v_count;
HDS_UDP_Recv_Verification_Attribute CALLBACK my_Verification_Func(char* recv_data,
																			  int recv_data_length,int total_data_length,HDS_UDP_Connection_ID *conID,
																			  sockaddr* remote_addr, socklen_t remote_addr_length)
{
	v_count++;
	if(v_count.value()>100000)
		int x=0;
	HDS_UDP_Recv_Verification_Attribute atti;
	atti._recv_buf=NULL;
	atti._recv_type = Verification_Recv_Type_Recv;
	atti._recv_failed_func=my_Recv_Failed_Func;
	xxID = *conID;
	return atti;
}

void test_simple_send_boundary()
{
#ifdef WIN32
	if(init_HDS_UDP(NULL,5150,4,4)==-1)
		return;
#else
	if(init_HDS_UDP("10.0.2.15",5150,4,4)==-1)
		return;
#endif

	sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_port=htons(5150);
#ifdef linux
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
#else
	addr.sin_addr.s_addr=inet_addr("127.0.0.1");
#endif

#ifdef _SUPPORT_ACE_
	regeditProcessor(1000, new send_link_Processor_boundary_ace());
#else
	regeditProcessor(1000, simpleProcessor_boundary);
#endif
	
	set_recv_verification_func(my_Verification_Func);

	char * data= new char[1491];
	for(int i=4;i<1491;i++)
		data[i]='i';
	int x=1000;
	memcpy(data,&x,4);
	for(int i=0;i<100000;i++)
	{

#ifdef _SUPPORT_HDS_UDP_2_
		if(CSend(data,1491,addr,NO_DELETE_AFTER_SEND)==-1)
#else
		if(CSend(data,1491,addr,NO_DELETE_AFTER_SEND,send_boundary_Sender_Handler)==-1)
#endif
			printf("error");
	}
	Sleep(6000);
	//fini_HDS_UDP();
	//stop_connection(&xxID);
	Sleep(100000000);
}

int HDS_UDP_API complexProcessor_boundary(HDS_UDP_Result_Process_Data * parm)
{
	//if(_succ_count++%100==0)
	printf("_count=%d\n",_succ_count++);
	char name[256];
#ifdef WIN32
	sprintf_s(name,256,"F:\\%s%dx.flv","movie",_recv_countb++);
#else
	sprintf(name,"F:\\%s%dx.mp3","movie",_recv_countb++);
#endif

	std::ofstream onf(name,std::ios::binary);
	if(onf)
	{
		onf.write(parm->data+4,parm->data_length-4);
		onf.close();
	}
	return 0;
}

void test_complex_send_boundary()
{
#ifdef WIN32
	if(init_HDS_UDP(NULL,5150,4,4)==-1)
		return;
#else
	if(init_HDS_UDP("10.0.2.15",5150,4,4)==-1)
		return;
#endif

	sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_port=htons(5150);
#ifdef linux
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
#else
	addr.sin_addr.s_addr=inet_addr("116.56.142.253");
#endif

#ifdef _SUPPORT_ACE_
	regeditProcessor(1000, new send_link_Processor_boundary_ace());
#else
	regeditProcessor(1000, complexProcessor_boundary);
#endif

	set_recv_verification_func(my_Verification_Func);

	std::ifstream inf( "F:\\GTO.12.FeiXue.x264-OGG.mkv",std::ios::binary);
	if(inf)
	{
		inf.seekg(0,std::ios::end);
		unsigned int length= inf.tellg();
		char *myblock =new char[length+4];
		int x=1000;
		memcpy(myblock,&x,4);
		inf.seekg(0,std::ios::beg);
		inf.read(myblock+4,length);
		Connection_ID xcon;
		printf("start sending\n");

#ifdef _SUPPORT_HDS_UDP_2_
		C_RSend(myblock,length+4,addr,xcon,NO_DELETE_AFTER_SEND);
#else
		C_RSend(myblock,length+4,addr,xcon,NO_DELETE_AFTER_SEND,send_boundary_Sender_Handler);
#endif
		
	}
	Sleep(3000);
	//fini_HDS_UDP();
	//printf("fini ok!\n");
	//stop_connection(&xxID);
	Sleep(100000000);
}

void test_message_node_send()
{
#ifdef WIN32
	if(init_HDS_UDP(NULL,5150,4,4)==-1)
		return;
#else
	if(init_HDS_UDP("192.168.0.200",5150,4,4)==-1)
		return;
#endif

	sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_port=htons(5150);
#ifdef linux
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
#else
	addr.sin_addr.s_addr=inet_addr("127.0.0.1");
#endif

#ifdef _SUPPORT_ACE_
	regeditProcessor(1000, new send_link_Processor_boundary_ace());
#else
	regeditProcessor(1000, send_link_Processor_boundary);
#endif
	set_recv_verification_func(my_Verification_Func);
	message_node* header = new message_node();
	int * mtype = new int();
	*mtype=1000;
	header->source_data = (char*)mtype;
	header->source_data_length = 4;
	header->del_type = DELETE_AFTER_SEND;

#ifndef _SUPPORT_HDS_UDP_2_
	header->send_handle = send_boundary_Sender_Handler;
#endif

	message_node* temp_node = header;
	for(int i=1;i<8000;i++)
	{
		char * data= new char[i];
		for(int j=0;j<i;j++)
		{
			data[j]='i';
		}
		message_node* node = new message_node();
		node->source_data = data;
		node->source_data_length = i;
		node->del_type = DELETE_AFTER_SEND;

#ifndef _SUPPORT_HDS_UDP_2_
		node->send_handle = send_boundary_Sender_Handler;
#endif
		temp_node->next = node;
		temp_node = node;
	}

	for(int i=0;i<1;i++)
	{
		Connection_ID con_ID;
		if(C_RSendLink(header,addr,con_ID))
		{
			printf("error\n");
		}
	}
	Sleep(6000);
	//fini_HDS_UDP();
	//stop_connection(&xxID);
	Sleep(100000000);
}

void test_method_process()
{
	HDS_UDP::HDS_UDP_Method_Processor::init_processor(8);
	Sleep(1000);
	HDS_UDP::HDS_UDP_Method_Processor::fini_porcessor();
	int xx=0;
}
