#include "../HDS_UDP_Kernel.h"
#include "../HDS_UDP_Kernel_Data_Filter.h"
#include "UDP_Socket_Server.h"
#include "../../common/atomic_t.h"
#include "../../common/HDS_Error.h"
#include "../../common/HDS_Socket_Utils.h"
#include "../../common/HDS_Thread.h"
#include "../../common/HDS_Blocking_Queue.h"
#include <boost/pool/singleton_pool.hpp>

static unsigned short _udp_port=0;
static SOCKET  _hSocket=0;
static sockaddr_in _internetAddr;
static atomic_t<long> _hexit=false;
static HDS_Thread_Data* _workerThreadDataArray=NULL;
static int  _numberOfWorkerThreads=0;
static HDS_Thread_Data _recvThreadData;
static atomic_t<unsigned long> _downloaded_count=0;
static atomic_t<unsigned long> _sended_count=0;

static THREAD_RETURN_VALUE HDS_THREAD_API UDPServerRecvThread(void *  lpParam);
static THREAD_RETURN_VALUE HDS_THREAD_API UDPServerWorkerThread(void * lpParam);

typedef boost::singleton_pool<Kernel_Recv_Data,sizeof(Kernel_Recv_Data)> _Recv_Data_Pool;
static HDS_Blocking_Queue<Kernel_Recv_Data*>  _recv_data_queue;

#ifdef WIN32
static inline int InitWinsock2()
{
	WSADATA wd = { 0 };
	if(WSAStartup(MAKEWORD(2, 2), &wd) != 0)
	{
		print_error("WSAStartup failed");
		return -1;
	}
	if(LOBYTE(wd.wVersion) != 2 || HIBYTE(wd.wVersion) != 2)
	{
		WSACleanup();
		print_error("WSAStartup failed to return the requested Winsock version.");
		return -1;
	}
	return 0;
}

static  inline void UnInitWinsock2()
{
	WSACleanup();// Cleanup Winsock
}
#endif

static inline void clean_up() 
{
#ifdef WIN32
	UnInitWinsock2();
#endif
}

int init_kernel(const char* ipaddr_,const unsigned short port_,const int udp_packet_filter_thread_num_)
{
#ifdef WIN32
	if(InitWinsock2()==-1)
	{
		print_error("WSAStartup failed to Init Winsock2.");
		return -1;
	}
#endif
	/* Initialize socket address structure for Interner Protocols */
	_numberOfWorkerThreads = udp_packet_filter_thread_num_;
	_udp_port = port_;
	memset(&_internetAddr, 0,sizeof(_internetAddr)); // empty data structure
	_internetAddr.sin_family = AF_INET;

	if(ipaddr_==NULL)
		_internetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		_internetAddr.sin_addr.s_addr=inet_addr(ipaddr_);

	_internetAddr.sin_port = htons(port_);
	_hSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if( bind(_hSocket, ( struct sockaddr * )&_internetAddr, sizeof(_internetAddr ) ) == -1 )
	{
		print_error("WSAStartup failed to bind sock.");
		close_socket(_hSocket);
		clean_up();
		return -1;
	}

	set_socket_recv_buf(_hSocket,16*1024);//HDS_UDP_SOCKET_RECV_BUF_SIZE);
	set_socket_send_buf(_hSocket,HDS_UDP_SOCKET_SEND_BUF_SIZE);

	if(set_socket_reuse(_hSocket) == -1)
	{
		print_error("WSAStartup failed to reuse sock.");
		close_socket(_hSocket);
		clean_up();
		return -1;
	}
	
	if(HDS_Thread_Create(UDPServerRecvThread,NULL,&_recvThreadData)==-1)
	{
		print_error("A recv thread was not able to be created");
		close_socket(_hSocket);
		clean_up();
		return -1;
	}

	_workerThreadDataArray = (HDS_Thread_Data*)malloc(_numberOfWorkerThreads*sizeof(HDS_Thread_Data));
	memset(_workerThreadDataArray, 0, sizeof(HDS_Thread_Data) * _numberOfWorkerThreads);
	for(int i= 0; i < _numberOfWorkerThreads; ++i)
	{
		// We only create in the suspended state so our workerThreadHandles object
		// is updated before the thread runs.
		if(HDS_Thread_Create(UDPServerWorkerThread,NULL,&_workerThreadDataArray[i])==-1)
		{
			print_error("A worker thread was not able to be created");
			close_socket(_hSocket);
			free(_workerThreadDataArray);
			clean_up();
			return -1;
		}
	}
	return 0;
}

THREAD_RETURN_VALUE HDS_THREAD_API UDPServerRecvThread(void* lpParam)
{
	Kernel_Recv_Data * rdata=NULL;
	while(!_hexit.value())
	{
		rdata = (Kernel_Recv_Data*)_Recv_Data_Pool::malloc();
		rdata->remote_addr_length=sizeof(sockaddr);
		if(rdata == NULL)
		{
			Sleep(100);
			break;
		}
		rdata->data_length  = recvfrom(_hSocket,rdata->data,KERNEL_RECV_DATA_BUF_LEN,0,
			&rdata->remote_addr,&rdata->remote_addr_length);
		if(rdata->data_length<=0)
		{
			_Recv_Data_Pool::free(rdata);
			break;
		}
		_downloaded_count.add(rdata->data_length);
		_recv_data_queue.enqueue(rdata); 
	}
	return 0;
}


THREAD_RETURN_VALUE HDS_THREAD_API UDPServerWorkerThread(void * parm)
{
	Kernel_Recv_Data * rdata=NULL;
	while(!_hexit.value())
	{
		if(_recv_data_queue.dequeue(rdata,-1) ==0)
		{
			kernel_filter_udp_data(rdata);
		    _Recv_Data_Pool::free(rdata);
		}
	}
	return 0;
}

extern "C"
int fini_kernel()
{
	_hexit=true;

	HDS_Thread_Join(&_recvThreadData);
	for(int i=0;i<_numberOfWorkerThreads;i++)
		HDS_Thread_Join(&_workerThreadDataArray[i]);

	close_socket(_hSocket);
	free(_workerThreadDataArray);
	Sleep(200);
	return 0;
}

extern "C"
int kernel_send(char* buf, unsigned int length,int flags, const struct sockaddr *to, socklen_t tolen)
{
	 _sended_count.add(length);
	 return sendto(_hSocket,buf,length,flags,to,tolen);
}

extern "C"
int get_udp_server_udp_port(unsigned short& port_)
{
	if(_udp_port == 0)
	{
		sockaddr_in sock_info;
		socklen_t sock_info_len=sizeof(sockaddr_in);
		getsockname(_hSocket,(sockaddr*)&sock_info,&sock_info_len);
		_udp_port = ntohs(sock_info.sin_port);
		return 0;
	}
	port_ = _udp_port;
	return 0;
}


extern "C"
unsigned long get_udp_server_downloaded_byte_count()
{
	return _downloaded_count.value();
}

extern "C"
unsigned long get_udp_server_uploaded_byte_count()
{
	return _sended_count.value();
}