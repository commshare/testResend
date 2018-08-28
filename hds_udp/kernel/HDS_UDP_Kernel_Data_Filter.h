#ifndef _HDS_UDP_DATA_FILTER_H_
#define _HDS_UDP_DATA_FILTER_H_

#include "../config/HDS_UDP_Kernel_Config.h"
#include "HDS_UDP_Kernel.h"

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define KERNEL_RECV_DATA_BUF_LEN MAX_UDP_PACKET_SIZE
typedef struct
{
	sockaddr remote_addr;
	socklen_t remote_addr_length;
	int      data_length;
	char     data[KERNEL_RECV_DATA_BUF_LEN];
}Kernel_Recv_Data;

void kernel_filter_udp_data(Kernel_Recv_Data *rdata);

#endif