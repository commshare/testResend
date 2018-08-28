#ifndef _HDS_ERROR_H_
#define  _HDS_ERROR_H_

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include "errno.h"
#endif

#include <stdio.h>

inline int get_last_error()
{
#ifdef WIN32
	return GetLastError();
#else
    return errno;
#endif
}

inline void print_error(const char * msg)
{
	printf("%s >>> errno=%d\n",msg,get_last_error());
}


#endif