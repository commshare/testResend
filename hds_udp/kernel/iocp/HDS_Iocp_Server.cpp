#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#include <MSWSock.h >
#include <assert.h>
#include "HDS_Iocp_Server.h"
#include "../../common/HDS_Thread.h"
#include "../../common/atomic_t.h"
#include "../../common/HDS_Error.h"
#include "../../common/HDS_Unistd.h"
#include "../../common/HDS_Socket_Utils.h"
#include "../../common/HDS_Event.h"
#include "../../common/s_list.h"
#include "../../buffer/HDS_Buffer_Pool.h"
#include "../HDS_UDP_Kernel_Data_Filter.h"

enum UDP_Overlapped_Event_Operator
{
	HDS_OPERATION_READ=0,
	HDS_OPERATION_WRITE=1  //no use
};

typedef  struct 
{
	OVERLAPPED overlapped;
	UDP_Overlapped_Event_Operator operation;
	Kernel_Recv_Data rdata;
}UDP_Overlapped_Event;


static unsigned short _udp_port=0;
static int _numberOfConcurrentThreads=0;
static int  _numberOfWorkerThreads=0;
static HANDLE _completionPort=0;
static HDS_Thread_Data * _workerThreadDataArray=0;
static SOCKET  _hSocket=0;
static sockaddr_in _internetAddr;
static atomic_t<long> _hexit=false;
static atomic_t<unsigned long> _downloaded_count=0;
static atomic_t<unsigned long> _sended_count=0;
static atomic_t<long> _post_recv_failed_count=0;
static CRITICAL_SECTION _send_buf_queue_mutex;
static HANDLE _send_buf_queue_event=0;
static s_list<UDP_Overlapped_Event*> _send_buf_queue;
static int _send_buf_need_wait_up=0;

static void post_recv(UDP_Overlapped_Event * event=NULL);

static THREAD_RETURN_VALUE HDS_THREAD_API UDPServerWorkerThread(LPVOID lpParam);


static UDP_Overlapped_Event* test_event=0;

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

int init_kernel(const char* ipaddr_,const unsigned short port_, const int udp_packet_filter_thread_num_)
{
	_udp_port = port_;

	_numberOfWorkerThreads = udp_packet_filter_thread_num_;
	_numberOfConcurrentThreads = get_cpu_cores_count();


	//init send buf
	if(HDS_Event_Init(&_send_buf_queue_event,false)==-1)
	{
		return -1;
	}
	InitializeCriticalSection(&_send_buf_queue_mutex);
	for(int i=0;i<_numberOfConcurrentThreads*16;i++)
	{
		UDP_Overlapped_Event* event = (UDP_Overlapped_Event*)malloc(sizeof(UDP_Overlapped_Event));
		_send_buf_queue.push_back(event);
		test_event = event;
	}

	_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, _numberOfConcurrentThreads);
	if(_completionPort == NULL)
	{
		print_error("Could not create the IOCP");
		return -1;
	}


	if(InitWinsock2()==-1)
	{
		CloseHandle(_completionPort);
		print_error("init winsock2 error");
		return -1;
	}

	_hSocket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(_hSocket == INVALID_SOCKET)
	{
		print_error("Could not create the main socket.");
		CloseHandle(_completionPort);
		closesocket(_hSocket);
		UnInitWinsock2();
		return -1;
	}

	set_socket_recv_buf(_hSocket,8*1024);//HDS_UDP_SOCKET_RECV_BUF_SIZE);
	set_socket_send_buf(_hSocket,HDS_UDP_SOCKET_SEND_BUF_SIZE);

	if(set_socket_reuse(_hSocket) == -1)
	{
		print_error("set no blocking error.");
		CloseHandle(_completionPort);
		closesocket(_hSocket);
		UnInitWinsock2();
	}

	if(set_socket_nonblocking(_hSocket) == -1)
	{
		print_error("set no blocking error.");
		CloseHandle(_completionPort);
		closesocket(_hSocket);
		UnInitWinsock2();
		return -1;
	}

	// Bind the socket to the requested port
	_internetAddr.sin_family = AF_INET;
	if(ipaddr_==NULL)
		_internetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		 _internetAddr.sin_addr.s_addr=inet_addr(ipaddr_);
	_internetAddr.sin_port = htons(_udp_port);

	if(bind(_hSocket, reinterpret_cast<PSOCKADDR>(&_internetAddr), sizeof(sockaddr_in))==SOCKET_ERROR)
	{
		print_error("Could not bind the main socket.");
		CloseHandle(_completionPort);
		closesocket(_hSocket);
		UnInitWinsock2();
		return -1;
	}

	// Connect the listener socket to IOCP
	if(CreateIoCompletionPort(reinterpret_cast<HANDLE>(_hSocket), _completionPort, 0, _numberOfConcurrentThreads) == 0)
	{
		print_error("Could not assign the socket to the IOCP handle");
		CloseHandle(_completionPort);
		closesocket(_hSocket);
		UnInitWinsock2();
		return -1;
	}

	// Create all of our worker threads
	_workerThreadDataArray = (HDS_Thread_Data*)malloc(_numberOfWorkerThreads*sizeof(HDS_Thread_Data));
	memset(_workerThreadDataArray, 0, sizeof(HDS_Thread_Data) * _numberOfWorkerThreads);
	for(int i= 0; i < _numberOfWorkerThreads; ++i)
	{
		// We only create in the suspended state so our workerThreadHandles object
		// is updated before the thread runs.
		if(HDS_Thread_Create(UDPServerWorkerThread,NULL,&_workerThreadDataArray[i])==-1)
		{
			print_error("A worker thread was not able to be created");
			CloseHandle(_completionPort);
			closesocket(_hSocket);
			free(_workerThreadDataArray);
			UnInitWinsock2();
			return -1;
		}
	}

	for(int i=0;i<_numberOfWorkerThreads*2;i++)
		post_recv();

	return 0;
}

#define reback_event_to_send_buffer(event) \
	do{\
	EnterCriticalSection(&_send_buf_queue_mutex);\
	_send_buf_queue.push_back(event);\
	if(_send_buf_need_wait_up==1){_send_buf_need_wait_up=0;SetEvent(_send_buf_queue_event);}\
	LeaveCriticalSection(&_send_buf_queue_mutex);\
	}while(0)

THREAD_RETURN_VALUE HDS_THREAD_API UDPServerWorkerThread(LPVOID lpParam)
{
	BOOL result = 0;
	DWORD numberOfBytes = 0;
	ULONG_PTR key = 0;
	OVERLAPPED * lpOverlapped = 0;
	UDP_Overlapped_Event * event = 0;
	UDP_Overlapped_Event* temp_event =0;
	int type=0;

	while(!_hexit.value())
	{
		// Wait for our IOCP notifications :) With this setup, we block indefinitely, so when we want
		// the thread to exit, we must use the PostQueuedCompletionStatus function with a special key
		// value of -1 to signal the thread should exit.
		result = GetQueuedCompletionStatus(_completionPort, &numberOfBytes, &key, &lpOverlapped, INFINITE);

		// We reserve a special event of having a passed key of -1 to signal an exit. In this case
		// we just want to break from our infinite loop. Since we are using pools to manage memory,
		// we do not have to worry about anything else here.
		if(key == -1)
		{
			break;
		}

		while(_post_recv_failed_count.value() >0)
		{
			_post_recv_failed_count--;
			post_recv();
		}

		// Obtain the UDPOverlappedEvent pointer based on the address we received from
		// GetQueuedCompletionStatus in the overlapped field. The simple theory is that
		// when we pass the overlapped field from a UDPOverlappedEvent object into 
		// WSARecvFrom, we get that pointer here, so we can calculate where the overlapped
		// field is inside the structure and move the pointer backwards to the start of the
		// object. This is the safe and preferred way of doing this task!
		event = CONTAINING_RECORD(lpOverlapped, UDP_Overlapped_Event, overlapped);

		// If the GetQueuedCompletionStatus call was successful, we can pass our data to the
		// thread for processing network data.
		if(result == TRUE)
		{
			// No key means no context operation data, so WSARecvFrom/WSASendTo generated
			// this event for us.
			if(key == 0)
			{
				switch(event->operation)
				{
				case HDS_OPERATION_READ:// WSARecvFrom event
					{
						event->rdata.data_length =  numberOfBytes;
						_downloaded_count.add(numberOfBytes);
						kernel_filter_udp_data(&event->rdata);
						post_recv(event);
						break;
					}
				case HDS_OPERATION_WRITE:
					{
						reback_event_to_send_buffer(event);
						break;
					}
				}
			}
			else
			{
				// We did something via PostQueuedCompletionStatus. We don't
				// have any specific code in this example, but if we do pass
				// something via the key parameter, we have to consider if it
				// needs to be cleaned up or not. We assume in this case we
				// will pass an event we need to free, that might not always
				// be the case though!
				post_recv(event);
			}
		}
		else
		{
			int error = GetLastError();
			// Free this connection data since we cannot process it in the appropriate thread.
			switch(event->operation)
			{
			case HDS_OPERATION_READ:// WSARecvFrom event
				{
					post_recv(event);
					break;
				}
			case HDS_OPERATION_WRITE:
				{
					reback_event_to_send_buffer(event);
					break;
				}
			}
		}
	}
	return 0;
}

#ifdef _LIMIT_UDP_SEND_SPEED_
static volatile long _send_speed_limit=0;
static volatile long _send_speed_limit_clock=0;
#endif

int kernel_send(char* buf, unsigned int length,int flags, const struct sockaddr *to, socklen_t tolen)
{
	_sended_count.add(length);
	if(length == HDS_UDP_PACKET_MAX_HEADER_LENGTH)
	{
		return sendto(_hSocket,buf,length,0,to,tolen);
	}
	UDP_Overlapped_Event* event;
	int res=-1;
	while(!_hexit.value())
	{
		EnterCriticalSection(&_send_buf_queue_mutex);
		if(_send_buf_queue.dequeue_head(event)==0)
		{
			LeaveCriticalSection(&_send_buf_queue_mutex);
			break;
		}
		else
		{
			if(_send_buf_need_wait_up==0)
			{
				_send_buf_need_wait_up=1;
				ResetEvent(_send_buf_queue_event);
			}
			LeaveCriticalSection(&_send_buf_queue_mutex);
			WaitForSingleObject(_send_buf_queue_event,INFINITE);
			continue;
		}
	}

	if(!_hexit.value())
	{
		DWORD dwSent = 0;
		event->operation = HDS_OPERATION_WRITE;
		memset(&event->overlapped, 0, sizeof(OVERLAPPED));
		event->rdata.remote_addr_length = tolen;
		memcpy(&event->rdata.remote_addr, to, tolen);
		event->rdata.data_length = length ;
		memcpy(&event->rdata.data, buf, length);
		WSABUF sendBufferDescriptor;
		sendBufferDescriptor.buf = reinterpret_cast<char *>(event->rdata.data);
		sendBufferDescriptor.len = event->rdata.data_length;
		res = WSASendTo(_hSocket,&sendBufferDescriptor,
			1,&dwSent, 
			0,&event->rdata.remote_addr,
			event->rdata.remote_addr_length, 
			&event->overlapped, NULL);
		if(res == SOCKET_ERROR) // We don't expect this, but...
		{
			res = WSAGetLastError();
			if(res != WSA_IO_PENDING) // This means everything went well
			{
				assert(res != WSA_IO_PENDING);
				// Since we could not send the data, we need to cleanup this
				// event so it's not dead weight in our memory pool.
				reback_event_to_send_buffer(event);
				return -1;
			}
		}
		return length;
	}
	return res;
	//we can use WSASendTo too;
}

int fini_kernel()
{
	_hexit = true;
	SetEvent(_send_buf_queue_event);
	Sleep(200);

	if(_completionPort != INVALID_HANDLE_VALUE)
	{
		// Destroy all of our worker threads by posting an IOCP event that signals the threads
		// to exit. There is no other way around having to do this that is more efficient!
		for(int x = 0; x < _numberOfWorkerThreads; ++x)
		{
			PostQueuedCompletionStatus(_completionPort, 0, static_cast<ULONG_PTR>(-1), 0);
			Sleep(1);// Try to give up our time slice so the other threads can exit
		}
	}

	if(_workerThreadDataArray)
	{
		// Wait for all worker threads to close
		for(int i = 0; i < _numberOfWorkerThreads; i++)
			HDS_Thread_Join(&_workerThreadDataArray[i]);

		delete [] _workerThreadDataArray;
		_workerThreadDataArray = 0;
	}

	if(_hSocket!= INVALID_SOCKET)
	{
		closesocket(_hSocket);
		_hSocket = INVALID_SOCKET;
	}

	if(_completionPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_completionPort);
		_completionPort = INVALID_HANDLE_VALUE;
	}

	UnInitWinsock2();
	return 0;
}

static void post_recv(UDP_Overlapped_Event * event)
{
	if(event == NULL)
	{
		event = (UDP_Overlapped_Event*)malloc(sizeof(UDP_Overlapped_Event));
	}

	memset(&event->overlapped, 0, sizeof(OVERLAPPED));
	// If WSARecvFrom is completed in an overlapped manner, it is the Winsock service provider's 
	// responsibility to capture the WSABUF structures before returning from this call. This enables 
	// applications to build stack-based WSABUF arrays pointed to by the lpBuffers parameter.
	WSABUF recvBufferDescriptor;
	recvBufferDescriptor.buf = reinterpret_cast<char *>(event->rdata.data);
	recvBufferDescriptor.len = KERNEL_RECV_DATA_BUF_LEN;
	event->rdata.remote_addr_length = sizeof(sockaddr_in);
	event->operation = HDS_OPERATION_READ;

	// Post our overlapped receive event on the socket. We expect to get an error in this situation.
	DWORD numberOfBytes = 0;
	DWORD recvFlags = 0;
	INT result = WSARecvFrom(_hSocket, 
		&recvBufferDescriptor, 1, &numberOfBytes, &recvFlags, 
		&event->rdata.remote_addr, &event->rdata.remote_addr_length, &event->overlapped, NULL);
	if(result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		// We expect to get an ERROR_IO_PENDING result as we are posting overlapped events
		if(error != ERROR_IO_PENDING)
		{
			switch(error)
			{
			case 10054:
				{
					DWORD dwBytesReturned = 0;
					BOOL bNewBehavior = FALSE;
					DWORD status = WSAIoctl(_hSocket, SIO_UDP_CONNRESET,
						&bNewBehavior, sizeof(bNewBehavior),
						NULL, 0, &dwBytesReturned,
						NULL, NULL);
					if (SOCKET_ERROR == status){
						DWORD dwErr = WSAGetLastError();
						if (WSAEWOULDBLOCK == dwErr)
						{// nothing to do
							return;
						}else{

#ifdef HDS_UDP_DEBUG
							printf("WSAIoctl(SIO_UDP_CONNRESET) Error: %d\n", dwErr);
#endif
							return;
						}
					}
					break;
				}
				// If we get here, then we are trying to post more events than 
				// we can currently handle, so we just break out of the loop
			case WSAENOBUFS:
				// Time to exit since the socket was closed. This should only happen
				// if Destroy is called.
			case WSAENOTSOCK:
				break;
				// Otherwise, we have some other error to handle
			default:
				{
					assert(error != ERROR_IO_PENDING);
				}
			}
			_post_recv_failed_count++;
			free(event);
		}
		// As expected
		else
		{
			// Add a pending overlapped events just posted
		}
	}
	else
	{
		// If we get here, we did not have enough pending receives posted on the socket.
		// This is ok because a worker thread will still process the data, it's just we
		// hit a burst operation (can be simulated by holding the console thread) and
		// we churned through the data. It is very important we do not free the 'event' 
		// pointer here! It *will* be handled by a WorkerThread. There is nothing we 
		// need to do here really.
	}
}


int get_udp_server_udp_port(unsigned short& port_)
{
	if(_udp_port == 0)
	{
		sockaddr_in sock_info;
		int sock_info_len=sizeof(sockaddr);
		getsockname(_hSocket,(sockaddr*)&sock_info,&sock_info_len);
		_udp_port = ntohs(sock_info.sin_port);
		return 0;
	}
	port_ = _udp_port;
	return 0;
}


unsigned long get_udp_server_downloaded_byte_count()
{
	return _downloaded_count.value();
}

unsigned long get_udp_server_uploaded_byte_count()
{
	return _sended_count.value();
}
#endif