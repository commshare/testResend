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
#include "../include/HDS_UDP.h"
#include <stdio.h>
#include <list>

atomic_t<long> value=0;
using namespace HDS_UDP;

THREAD_RETURN_VALUE  HDS_THREAD_API mycallback(void* lpParam)
{
	value++;
	if(value.value()%100==0)
		printf("%d clock=%d\n",value.value(),clock());
	if(value.value()>100000)
		printf("%d clock=%d\n",value.value(),clock());
	return 0;
}

void test_bitmap()
{
	Atomic_Bitmap bm(100);
	int res = bm.set(92);
	bm.set(97);

	for(int i=0;i<89;i++)
		bm.set(i);

	int pos = bm.findFirstSetBit(93);
	pos = bm.findFirstSetBit(0);
	pos = bm.findFirstSetBit(33);

	pos = bm.findFirstUnsetBit(0);

	for(int i=90;i<92;i++)
		bm.set(i);

	bm.set(99);
	bm.set(98);
	pos= bm.findFirstSetBit_R(99);
	pos = bm.findFirstUnsetBit_R(99);
}

void test_time_wheel()
{

	HDS_timer_wheel *wl = new HDS_timer_wheel(2);

	for(int i=0;i<100000;i++)
	{
		wl->add_timer(10,mycallback,(void*)i);
		if(i%100==0)
			printf("add time = %d\n",clock());
		Sleep(10);
	}
	Sleep(3000);
	printf("%d clock=%d\n",value.value(),clock());
	//wl->exit();
	//timeval nox;
	//gettimeofday(&nox,NULL);
	//Sleep(65536);
	//timeval dif;
	//gettimeofday(&dif,NULL);
	//dif = timevalue_sub(&dif,&nox);
	//uint64_t uv = dif.tv_sec*1000+dif.tv_usec/1000;
	//long tix = static_cast<long>(uv & 0x00000000fffffffff);

	//Sleep(65536);
	printf("---add time = %d\n",clock());
	wl->add_timer(65587,mycallback,NULL);
	wl->add_timer(65587*2,mycallback,NULL);
	wl->add_timer(65587*3,mycallback,NULL);
	wl->add_timer(655,mycallback,NULL);
	wl->add_timer(65587*4,mycallback,NULL);
	Sleep(10000000);
}

void test_map()
{
	clock_t x1 = clock();
	HP_map<int ,int> mym;
	for(int i=0;i<10000000;i++)
		mym.insert(i,i);

	for(int i=0;i<10000000;i++)
	{
		int res=0;
		mym.find(i,res);
		if(res!=i)
			printf("error\n");
	}
	clock_t x2 = clock();
	printf("clock=%d\n",x2-x1);
}

void test_set()
{
	clock_t x1 = clock();
	HP_set<int> mys;
	for(int i=0;i<10000000;i++)
		mys.insert(i);

	for(int i=0;i<10000000;i++)
	{
		if(mys.find(i)==-1)
			printf("error\n");
	}
	clock_t x2 = clock();
	printf("clock=%d\n",x2-x1);
}

void test_queue()
{
	HDS_Blocking_Queue<int> qu;
	qu.enqueue(1);
	int x;
	qu.dequeue(x,1000);
}


static int thread_id=0;
THREAD_RETURN_VALUE HDS_THREAD_API myth(void * ui)
{
	int id = thread_id++;
	while(true)
	{
		int x;
		HDS_Blocking_Queue<int>* qu = (HDS_Blocking_Queue<int>*)(ui);
 		if(qu->dequeue(x,10000)==-1)
		{
			printf("time out thread id =%d\n",id);
		}
		else
		{
			if(x % 2==0)
			{
				//Sleep(100);
				printf("x value=%d  id = %d \n",x, id);
			}
		}
		int y=0;
	}
}

void test_block_queue()
{
	HDS_Blocking_Queue<int> qu;
	HDS_Thread_Data d,e;
	HDS_Thread_Create(myth,&qu,&d);
	HDS_Thread_Create(myth,&qu,&e);
	int x =0;
	qu.enqueue(x++);
	while(true)
	{
		for(int i=0; i<= 20000; i++)
		{
			qu.enqueue(x++);
		}
		Sleep(8000);
	}
	//Sleep(100000);
}


void test_udp_server()
{
	init_kernel("127.0.0.1",5150,4);
}

void test_kernel()
{
	if(init_kernel("127.0.0.1",5150,4)==-1)
		return;

	char data[12]="hello world";

	sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_port=htons(5150);
#ifdef linux
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
#else
	addr.sin_addr.s_addr=inet_addr("127.0.0.1");
#endif

	for(int i=0;i<1000;i++)
	{
		int res = kernel_send(data,12,0,reinterpret_cast<sockaddr *>(&addr),sizeof(sockaddr_in));
		if(res ==-1)
			print_error("kernel_send error");
		Sleep(1);
	}
	Sleep(10000000);
	//fini_kernel();
	//printf("fini kernel ok\n");
}

void test_atomic()
{
	atomic_t<long> x=10;
	printf("--x =%d",--x);
	printf("++x=%d",++x);
	printf("x--=%d",x--);
	printf("x++=%d",x++);
	x=0;
	x.AtomicBitTestAndSet(10);
	printf("x=%d\n",x.value());
	x.AtomicBitTestAndReset(10);
	printf("x=%d\n",x.value());
	x.AtomicBitTestAndSet(0);
	printf("x=%d\n",x.value());
	x.AtomicBitTestAndReset(0);
	printf("x=%d\n",x.value());
	x.AtomicBitTestAndSet(31);
	printf("x=%d\n",x.value());
	x.AtomicBitTestAndReset(31);
	printf("x=%d\n",x.value());
	x.AtomicBitTestAndSet(20);
	printf("x=%d\n",x.value());
	x.AtomicBitTestAndReset(20);
	printf("x=%d\n",x.value());
	x.AtomicBitTestAndSet(15);
	printf("x=%d\n",x.value());
	x.AtomicBitTestAndReset(15);
	printf("x=%d\n",x.value());
}

//void test_buffer_pool()
//{
//	char * buf;
//	for(int i=0;i<266;i++)
//	{
//		for(int j=0;j<64;j++)
//		{
//			if(HDS_UDP::Buffer_Pool::get_buffer(buf,i)==-1)
//			{
//				printf("error");
//				return;
//			}
//			printf("need size=%d \n",i);
//			HDS_UDP::Buffer_Pool::release_buffer(buf,i);
//		}
//	}
//}

void test_connectionID()
{
	HDS_UDP::Connection_ID cid1,cid2;
	//cid1.create_connectionID((HDS_UDP::connection*)1);
	//cid2.create_connectionID((HDS_UDP::connection*)1);
	init_HDS_UDP(NULL,5150);
	for(int i=0; i<=1024*1024;i++)
	{
		if(i== 1024*1024)
			int x=0;
		if(cid1.create_connectionID((HDS_UDP::connection*)2)==-1)
			printf("error");
		if(i==0 || i== 1024*1024-1)
			printf("id%d\n",cid1._CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT);
		HDS_UDP::connection* ui = HDS_UDP::connection::get_connection(cid1._CID._ID32[0]&HDS_UDP_CONNECTION_OFFSET_SHIRT);
		if(ui ==0)
			int xxx=0;
	}
	int y=0;
}

atomic_t<long> _recv_count=0;

#ifdef _SUPPORT_ACE_
class ss_processor:public I_UDP_Processor
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
int HDS_UDP_API simpleProcessor(HDS_UDP_Result_Process_Data * parm)
{
	int res = query_separate_recv_buf(parm);
	printf("%s,%d\n",&parm->data[4],_recv_count++);
	return 0;
}
#endif

void test_udp()
{
	if(init_HDS_UDP("127.0.0.1",5150,4,4)==-1)
		return;

	char data[12]="hello world";
	char sdata[16];
	int * d = (int*)sdata;
	*d = 100;
	memcpy((char*)&sdata[4],(char*)&data,12);

#ifdef _SUPPORT_ACE_
	regeditProcessor(100,new ss_processor);
#else
	regeditProcessor(100, simpleProcessor);
#endif



	sockaddr_in addr;
	addr.sin_family=AF_INET;
	addr.sin_port=htons(5150);
#ifdef linux
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
#else
	addr.sin_addr.s_addr=inet_addr("127.0.0.1");
#endif

	for(int i=0;i<1000000;i++)
	{
		int res = kernel_send(sdata,16,0,reinterpret_cast<sockaddr *>(&addr),sizeof(sockaddr_in));
		if(res ==-1)
			print_error("kernel_send error");
		//Sleep(1);
	}
	Sleep(10000000);
	fini_kernel();
	//printf("fini kernel ok\n");
}


void test_recv_bitmap()
{
	HDS_UDP::HDS_UDP_Connection_Ack_Info_Collector bm;
	int oldvalue;
	//bm.set(128*32*HDS_UDP_PACKET_MAX_DATA_LENGTH);
	for(int i =0;i<128*9;i++)
	{
		if(i==23 || i==(128*8-21))continue;
		bm.set(i*HDS_UDP_PACKET_MAX_DATA_LENGTH,i,&oldvalue);
	}
	unsigned int tmseq;
	int tm;
	HDS_UDP::Ack_Type type = bm.get_next_ack(tmseq,tm);
	unsigned int pos =0;
	bm.set(23*HDS_UDP_PACKET_MAX_DATA_LENGTH,12,&oldvalue);
	bm.set((128*8+19)*HDS_UDP_PACKET_MAX_DATA_LENGTH,13,&oldvalue);
	bm.set((128*8+190)*HDS_UDP_PACKET_MAX_DATA_LENGTH,14,&oldvalue);
	bm.set((128*8*2)*HDS_UDP_PACKET_MAX_DATA_LENGTH,15,&oldvalue);
	HDS_UDP::Ack_Type ty = bm.get_next_ack(pos,tm);
	int f=0;
}


HDS_UDP::connection *conxx;
THREAD_RETURN_VALUE HDS_THREAD_API test_set_lf(void * ui)
{
	int res =0;
	for(int i=0;i<10000000;i++)
	{
		set_connection_left_window_cursor(conxx,i,res);
		if(res == -1)
			printf("res=-1 i=%d\n",i);
	}
	return 0;
}


typedef HDS_UDP::HDS_Singleton_Pool<8> mypool;
void test_pool()
{
	{
		//HDS_UDP::_pool_base_ base;
		//base.init(8);
		//char * a=0;
		//for(int i=0;i<1024*1024;i++)
		//{
		//	a = base.get_object();
		//}
	}

	HDS_UDP::Buffer_Pool::init();
	{
		char * df=(char*)HDS_UDP::Buffer_Pool::get_buffer(513);
		HDS_UDP::Buffer_Pool::release_buffer(df,513);
		int x=0;
	}
	{
		char * x;
		for(int i=0;i<1024*1024;i++)
		{
			x = (char*)mypool::malloc();
			mypool::free(x);
		}
	}

	{
		char ** a = new char*[1024*1024];
		for(int i=0;i<1024*1024;i++)
		{
			a[i] = (char*)mypool::malloc();
		}
		for(int i=0;i<1024*1024;i++)
		{
			mypool::free(a[i]);
		}
		for(int i=0;i<1024*1024;i++)
		{
			a[i] = (char*)mypool::malloc();
		}
		for(int i=0;i<1024*1024;i++)
		{
			mypool::free(a[i]);
		}
		char ** b = new char*[1024*512];
		for(int i=0;i<1024*512;i++)
		{
			a[i] = (char*)mypool::malloc();
		}
		for(int i=1024*512;i>=0;i--)
			mypool::free(a[i]);

		for(int i=0;i<1024*512;i++)
		{
			b[i] = (char*)mypool::malloc();
		}

		for(int i=0;i<1024*512;i++)
		{
			if(b[i]!=a[i])
				break;
		}
	}

	int x=0;
}

void test_set_connection_left_window_cursor()
{
	HDS_Thread_Data dat;
	conxx = new HDS_UDP::connection();
	conxx->_left_window_cursor=0;
	//HDS_Thread_Create(test_set_lf,NULL,&dat);
	int res =0;
	for(int i=0;i<10000000;i=i++)
	{
		set_connection_left_window_cursor(conxx,i,res);

		//long base_seq=i;long lseq = i;long olseq = i;
		//do{
		//	lseq = olseq;
		//	olseq =conxx->_left_window_cursor.AtomicExchange(lseq);
		//}while(olseq > lseq);
		//		res = lseq<=base_seq?0:-1;

		if(res == -1)
			printf("res=-1 i=%d\n",i);
	}
	printf("left_windows %d",conxx->_left_window_cursor);
	HDS_Thread_Join(&dat);
}