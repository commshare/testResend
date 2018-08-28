#ifndef  _HDS_SOCKET_UTILITY_H_ 
#define _HDS_SOCKET_UTILITY_H_


#include <fcntl.h>

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
 #include <arpa/inet.h>
#endif

static inline int set_socket_nonblocking(int sockfd) 
{ 
#ifdef WIN32
	    unsigned long nonblocking = 1;
	    ioctlsocket(sockfd, FIONBIO, (unsigned long*) &nonblocking );
#else
		if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) 
			return -1; 
#endif
		return 0;
}

static inline int set_socket_broadcast(int fd)
{
	// Allow LAN based broadcasts since this option is disabled by default.
	int value = 1;
	return setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char *)&value, sizeof(int));
}

static inline int set_socket_blocking( int fd )
{
#ifdef WIN32
	unsigned long nonblocking = 0;
	if(ioctlsocket( fd, FIONBIO, (unsigned long*) &nonblocking )!=0)
		return -1;
#else
	int flags;
	flags = fcntl( fd, F_GETFL );
	if( flags < 0 ) return flags;
	flags &= ~O_NONBLOCK;
	if( fcntl( fd, F_SETFL, flags ) < 0 ) return -1;
#endif
	return 0;
}

static inline int set_socket_reuse(int fd)
{
	int opt=SO_REUSEADDR;
#ifdef WIN32
	if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(char*)&opt,sizeof(opt))!=0)
		return -1;
#else
	if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(char*)&opt,sizeof(opt))!=0)
		return -1;
#endif
	return 0;
}

static inline void set_socket_recv_buf(int fd,const int buf_size)
{
	int nRecvBuf=buf_size;
	setsockopt(fd,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));
}

static inline void set_socket_send_buf(int fd,const int buf_size)
{
	int nSendBuf= buf_size;
	setsockopt(fd,SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBuf,sizeof(int));
}

static inline int sockaddrcmp(sockaddr *addra,socklen_t addra_length, sockaddr* addrb, socklen_t addrb_length)
{
	if(addra_length !=addrb_length) 
		return -1;
	unsigned char * a = (unsigned char*)addra;
	unsigned char * b = (unsigned char*)addrb;
	for(socklen_t i=0;i<addra_length;i++)
	{
		if(a[i] != b[i])
			return -1;
	}
	return 0;
};

#ifndef WIN32
typedef int SOCKET;
#endif

static inline void close_socket(SOCKET _hsock)
{
#ifdef WIN32
	closesocket(_hsock);
#else
	close(_hsock);
#endif
}
#endif