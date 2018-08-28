#include "HDS_Epoll_Server.h"
#include "../../common/HDS_Thread.h"
#include "../../common/atomic_t.h"
#include "../../common/HDS_Blocking_Queue.h"
#include "../../common/HDS_Mutex.h"
#include "../HDS_UDP_Kernel_Data_Filter.h"
#include "../../common/HDS_Unistd.h"
#include "../../common/HDS_Error.h"
#include "../../common/HDS_Socket_Utils.h"
#include <sys/epoll.h>
#include <sys/resource.h>
#include <string.h>
#include <unistd.h>
#include <boost/pool/singleton_pool.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXEPOLLSIZE 1
#define EPOLL_KERNEL_MAX_RECV_DATA_ARRAY 102400

static int _sock, _epoll_fd;  
static short  _port;  
static struct epoll_event _event;
static struct epoll_event _ev;
static HDS_Thread_Data _epoll_loop_thread_data;
static HDS_Thread_Data* _worker_thread_data;
static int              _worker_thread_count;
static atomic_t<long> _hexit;
static atomic_t<unsigned long> _downloaded_count=0;
static atomic_t<unsigned long> _sended_count=0;
static HDS_Mutex _kernel_send_mutex;

#ifdef EPOLL_RECV_WITH_NO_MUTEX
static atomic_t<Kernel_Recv_Data*> _kernel_recv_data_array[EPOLL_KERNEL_MAX_RECV_DATA_ARRAY];
static atomic_t<long> _write_kernel_recv_data_array_offset=0;
static atomic_t<long> _read_kernel_recv_data_array_offset=0;
#else
static HDS_Blocking_Queue<Kernel_Recv_Data*>  _recv_data_queue;
#endif

typedef boost::singleton_pool<Kernel_Recv_Data,sizeof(Kernel_Recv_Data)> Epoll_Recv_Data_Pool;

static THREAD_RETURN_VALUE HDS_THREAD_API epoll_loop(void * parm);

static THREAD_RETURN_VALUE HDS_THREAD_API worker_loop(void * parm);

int init_kernel(const char* ipaddr_,const unsigned short port_,const int udp_packet_filter_thread_num)
{
    if(ipaddr_ == NULL)
		return -1;

    _port = port_;
	HDS_Mutex_Init(&_kernel_send_mutex);

#ifdef EPOLL_RECV_WITH_NO_MUTEX
	memset(_kernel_recv_data_array,0,sizeof(Kernel_Recv_Data*)*EPOLL_KERNEL_MAX_RECV_DATA_ARRAY);
#endif

    _sock = socket(AF_INET,SOCK_DGRAM,0);
    if(_sock < 0)
    {
        print_error("create socket error");
        return -1;
    }

    if(set_socket_reuse(_sock)==-1)
        print_error("set socket reuse error");

    if(set_socket_nonblocking(_sock)==-1)
    {
        print_error("set socket nonblocking error");
        close(_sock);
        return -1;
    }

    struct sockaddr_in _local_addr;
    memset(&_local_addr,0,sizeof(struct sockaddr_in));

    _local_addr.sin_family = AF_INET;
    _local_addr.sin_port = htons(_port);
    _local_addr.sin_addr.s_addr=inet_addr(ipaddr_);
    if(bind(_sock,(struct sockaddr*)(&_local_addr),sizeof(struct sockaddr))==-1)
    {
        print_error("bind socket error");
        close(_sock);
        return -1;
    }

    _epoll_fd = epoll_create(MAXEPOLLSIZE);
    if(_epoll_fd <0 )
    {
        print_error("create epoll error");
        close(_sock);
        return -1;
    }


    _ev.events= EPOLLIN|EPOLLET;
    _ev.data.fd = _sock;

    if(epoll_ctl(_epoll_fd,EPOLL_CTL_ADD,_sock,&_ev)<0)
    {
        print_error("epoll control add error");
        close(_sock);
        close(_epoll_fd);
        return -1;
    }

    //create epoll loop thread
    pthread_attr_t attr;
    int rs = pthread_attr_init(&attr);
    if(rs == 0)
    {
        int policy;
        rs = pthread_attr_getschedpolicy(&attr,&policy);
        int priority = sched_get_priority_max(policy);
        struct sched_param sparm;
        sparm.__sched_priority = priority;
        if(priority != -1)
        {
            pthread_attr_setschedparam(&attr,&sparm);
        }
        if(pthread_create(&_epoll_loop_thread_data.dwThreadId,&attr,epoll_loop,0) !=0)
        {
            print_error("create epoll loop error");
            close(_sock);
            close(_epoll_fd);
            return -1;
        }
        pthread_attr_destroy(&attr);
    }
    else
    {
            if(HDS_Thread_Create(epoll_loop,0,&_epoll_loop_thread_data)!=0)
			{
				print_error("create epoll loop error");
				close(_sock);
				close(_epoll_fd);
				return -1;
			}
    }

    _worker_thread_count = udp_packet_filter_thread_num;
    if(_worker_thread_count == 0)
        _worker_thread_count = get_cpu_cores_count();
    
    _worker_thread_data = (HDS_Thread_Data*)malloc(sizeof(HDS_Thread_Data)* _worker_thread_count);
    for(int i=0;i< _worker_thread_count;i++)
    {
        if(HDS_Thread_Create(worker_loop,0,&_worker_thread_data[i])==-1)
        {
            print_error("create worker thread error");
            close(_sock);
            close(_epoll_fd);
            return -1;
        }
    }
	return 0;
}

THREAD_RETURN_VALUE HDS_THREAD_API epoll_loop(void * parm)
{
    int nfds;
    Kernel_Recv_Data * rdata;

#ifdef EPOLL_RECV_WITH_NO_MUTEX
	long offset = _write_kernel_recv_data_array_offset++;
#endif

    while(!_hexit.value())
    {
        nfds = epoll_wait(_epoll_fd,&_event,MAXEPOLLSIZE,-1);
        if(nfds == -1)
        {
            if(get_last_error() == EINTR)
			{
				continue;
			}
            
            print_error("epoll wait error");
            break;
        }

		while(!_hexit.value())
		{
			rdata = (Kernel_Recv_Data*)Epoll_Recv_Data_Pool::malloc();
			rdata->remote_addr_length=sizeof(sockaddr);
			if(rdata == NULL)
			{
				usleep(100);
				break;
			}
			rdata->data_length  = recvfrom(_sock,rdata->data,KERNEL_RECV_DATA_BUF_LEN,0,
				&rdata->remote_addr,&rdata->remote_addr_length);
			if(rdata->data_length<=0)
			{
				Epoll_Recv_Data_Pool::free(rdata);
				break;
			}
			_downloaded_count.add(rdata->data_length);

			//add to array
#ifdef EPOLL_RECV_WITH_NO_MUTEX
			if(_kernel_recv_data_array[offset].CompareExchange(rdata,0) !=0)
			{
				Epoll_Recv_Data_Pool::free(rdata);
				break;
			}
			else
			{
				_write_kernel_recv_data_array_offset.CompareExchange(0,EPOLL_KERNEL_MAX_RECV_DATA_ARRAY);
				offset = _write_kernel_recv_data_array_offset++;
			}
#else
			_recv_data_queue.enqueue(rdata); 
#endif
		}
    }
    return 0;
}

THREAD_RETURN_VALUE HDS_THREAD_API worker_loop(void * parm)
{
    Kernel_Recv_Data * rdata;

#ifdef EPOLL_RECV_WITH_NO_MUTEX
	long offset=_read_kernel_recv_data_array_offset++;
#endif

    while(!_hexit.value())
    {
#ifdef EPOLL_RECV_WITH_NO_MUTEX
		rdata= _kernel_recv_data_array[offset].AtomicExchange(0);
		if(rdata == 0)
		{
			Sleep(10);
		}
		else
		{
			kernel_filter_udp_data(rdata);
			Epoll_Recv_Data_Pool::free(rdata);
			_read_kernel_recv_data_array_offset.CompareExchange(0,EPOLL_KERNEL_MAX_RECV_DATA_ARRAY);
			offset = _read_kernel_recv_data_array_offset++;
		}
#else
		if(_recv_data_queue.dequeue(rdata,-1) ==0)
		{
			kernel_filter_udp_data(rdata);
			Epoll_Recv_Data_Pool::free(rdata);
		}
#endif
    }
    return 0;
}

int fini_kernel()
{
    _hexit=true;
    for(int i=0;i<_worker_thread_count;i++)
        HDS_Thread_Join(&_worker_thread_data[i]);

    //HDS_Thread_Join(&_epoll_loop_thread_data);
    epoll_ctl(_epoll_fd,EPOLL_CTL_DEL,_sock,NULL);
    close(_sock);
    close(_epoll_fd);
    free(_worker_thread_data);

	Sleep(200);
	HDS_Mutex_Delete(&_kernel_send_mutex);
	return 0;
}

#ifdef _LIMIT_UDP_SEND_SPEED_
static volatile long _send_speed_limit=0;
static volatile long _send_speed_limit_clock=0;
#endif

int kernel_send(char* buf, unsigned int length,int flags, const struct sockaddr *to, socklen_t tolen)
{
    _sended_count.add(length);
	int res = -1;
	if(!_hexit.value())
	{
		HDS_Mutex_Lock(&_kernel_send_mutex);

#ifdef _LIMIT_UDP_SEND_SPEED_
		long current_clock=HDS_UDP_clock();
		if(current_clock - _send_speed_limit_clock<3)
		{
			_send_speed_limit+=length;
			if(_send_speed_limit > LIMIT_500_US_SEND_SEPPD)
			{
				_send_speed_limit=0;
				usleep(500);
				_send_speed_limit_clock= current_clock;
				_send_speed_limit=0;
			}
		}
		else
		{
			_send_speed_limit_clock= current_clock;
			_send_speed_limit=0;
		}
#endif

		res = sendto(_sock,buf,length,flags,to,tolen);
		HDS_Mutex_Unlock(&_kernel_send_mutex);
	}
    return res;
}

int get_udp_server_udp_port(unsigned short & port_)
{
    if(_port == 0)
    {
        sockaddr_in sock_info;
        socklen_t sock_info_len=sizeof(sockaddr_in);
        getsockname(_sock,(sockaddr*)&sock_info,&sock_info_len);
        _port = ntohs(sock_info.sin_port);
		return 0;
    }
	port_ = _port;
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
