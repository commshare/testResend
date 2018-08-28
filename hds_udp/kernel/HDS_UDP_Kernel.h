#ifndef _HDS_UDP_KERNEL_H_
#define _HDS_UDP_KERNEL_H_

#include "../common/HDS_Unistd.h"

/*
@here define the function that kernel must support
@author jsunp
@Date 2012.5
*/

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <netinet/in.h>
#include <stddef.h>
#include <inttypes.h>
#endif

extern "C"
int init_kernel(const char* ipaddr_,const unsigned short port_,const int udp_packet_filter_thread_num);

extern "C"
int fini_kernel();

extern "C"
int kernel_send(char* buf, unsigned int length,int flags, const struct sockaddr *to, socklen_t tolen);

extern "C"
int get_udp_server_udp_port(unsigned short& port_);


extern "C"
unsigned long get_udp_server_downloaded_byte_count();

extern "C"
unsigned long get_udp_server_uploaded_byte_count();


#endif