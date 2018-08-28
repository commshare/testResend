#ifndef _HDS_UNISTD_H_
#define _HDS_UNISTD_H_

#ifdef WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <netinet/in.h>
#endif

#include <stddef.h>
#include <string.h>


#ifdef linux
#define CALLBACK 
#endif

#ifndef WIN32
#define INFINITE -1
typedef unsigned int DWORD;
#endif

#ifdef WIN32
typedef int socklen_t;
#endif

#ifdef WIN32
#define HDS_UDP_EXPORT_API  __declspec(dllexport) __stdcall
#else
#define HDS_UDP_EXPORT_API
#endif

#ifndef WIN32
typedef sockaddr_in SOCKADDR_IN;
#endif

#ifdef WIN32
#define  HDS_UDP_EXPORT __declspec(dllexport)
#else
#define  HDS_UDP_EXPORT 
#endif

#ifdef WIN32
#define HDS_UDP_API WINAPI 
#else
#define HDS_UDP_API
#endif

#ifdef WIN32
#include <time.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

//Sleep
#ifndef WIN32
#define Sleep(u) usleep(u*1000)
#endif


#define GET_STRUCT_FROM_CONTAININT_RECORD(address, type, field)\
	((type*)((PCHAR)(address) - (PCHAR)(&((type*)0)->field)))

//clock 在linux下clock只是计算了程序所占用的cpu时间。
#ifdef WIN32
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
struct timezone
{
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};
// Definition of a gettimeofday function
static int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	// Define a structure to receive the current Windows filetime
	FILETIME ft;

	// Initialize the present time to 0 and the timezone to UTC
	unsigned __int64 tmpres = 0;
	static int tzflag = 0;

	if (NULL != tv)
	{
		GetSystemTimeAsFileTime(&ft);

		// The GetSystemTimeAsFileTime returns the number of 100 nanosecond 
		// intervals since Jan 1, 1601 in a structure. Copy the high bits to 
		// the 64 bit tmpres, shift it left by 32 then or in the low 32 bits.
		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		// Convert to microseconds by dividing by 10
		tmpres /= 10;

		// The Unix epoch starts on Jan 1 1970.  Need to subtract the difference 
		// in seconds from Jan 1 1601.
		tmpres -= DELTA_EPOCH_IN_MICROSECS;

		// Finally change microseconds to seconds and place in the seconds value. 
		// The modulus picks up the microseconds.
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	if (NULL != tz)
	{
		if (!tzflag)
		{
			_tzset();
			tzflag++;
		}

		// Adjust for the timezone west of Greenwich
		long tzone;
		_get_timezone( &tzone );
		tz->tz_minuteswest =  tzone / 60;
		int daylight; 
		_get_daylight( &daylight );
		tz->tz_dsttime = daylight;

		/*		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight*/;
	}
	return 0;
}
#endif


#define timeval_sub(t1,t2) \
	do{\
	t1.tv_usec-=t2.tv_usec;\
	if((t1.tv_usec)<0){\
	--t1.tv_sec;\
	t1.tv_usec+=1000000;\
	}\
	t1.tv_sec-=t2.tv_sec;\
	}while(0)


typedef unsigned long long int utimevalue;

#ifndef __LP64__
typedef unsigned long long int uint64_t;
#endif

static inline int HDS_UDP_clock()
{
#ifdef WIN32
	return clock();
#else
	timeval dif;
	gettimeofday(&dif,NULL);
	uint64_t uv = dif.tv_sec*1000+dif.tv_usec/1000;
	return (int)(uv & 0xffffffff);
#endif
}

static inline int get_cpu_cores_count()
{
#ifdef WIN32
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);
	return SystemInfo.dwNumberOfProcessors;
#else
	return sysconf(_SC_NPROCESSORS_CONF);
#endif
};

enum IPADDR_TYPE
{
	IPV4,
	IPV6
};

static IPADDR_TYPE check_ip_addr_type(const char* ipaddr_)
{
	int length = strlen(ipaddr_);
	for(int i=0;i<length;i++)
	{
		if(ipaddr_[i]==':')
			return IPV6;
	}
	return IPV4;
}


#define HDS_ASSERT(expr)                                      \
	do {                                                     \
	if (!(expr)) {                                          \
	fprintf(stderr,                                       \
	"Assertion failed in %s on line %d: %s\n",    \
	__FILE__,                                     \
	__LINE__,                                     \
#expr);                                       \
	abort();                                              \
	}                                                       \
	} while (0)

#define container_of(ptr, type, member) \
	((type *) ((char *) (ptr) - offsetof(type, member)))

#endif