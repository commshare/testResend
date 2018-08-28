#include "../common/atomic_t.h"
#include "../common/AutoLock.h"
#include "../common/HDS_Event.h"
#include "../common/HDS_Timer_Wheel.h"
#include "../common/HDS_Unistd.h"
#include "../common/Atomic_Bitmap.h"
#include "test_base.h"
#include "test_simple_send.h"
//#include "test_tcp.h"
#include <stdlib.h> 
#include <iostream> 
#include <time.h> 

int main(int argc, char* argv[])
{
	//test_set_connection_left_window_cursor();
	Atomic_Bitmap bm(32);
	bm.set(0);
	int i=bm.findFirstSetBit(0);
	timeval val,val1;
	gettimeofday(&val,NULL);

	int xyt = 1<<HDS_UDP_CONNECTION_MARK_BITS;
	int dfd = (1<<HDS_UDP_CONNECTION_MARK_BITS)-1;
	gettimeofday(&val1,NULL);
	srand( (unsigned)time( NULL ) );
	atomic_t<long> x(100);
	int res =0;
	//test_tcp();
	//test_message_node_send();
	//test_simple_send_boundary();
	//test_complex_send_boundary();
	test_simple_send_size_8M_1();
	//test_pool();
	//test_recv_bitmap();
	//test_atomic();
	//test_method_process();
	//test_udp();
	 //test_connectionID();
	//test_buffer_pool();
    //test_block_queue();
	//while(true)
//	{
//			Sleep(1000000);
//	}
	return 0;
}
