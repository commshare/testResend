#include "HDS_UDP_Kernel_Data_Filter.h"
#include "../common/atomic_t.h"
#include "../connection/HDS_UDP_Connection.h"
#include <stdio.h>

void kernel_filter_udp_data(Kernel_Recv_Data *rdata)
{
	HDS_UDP::connection::filter_data(rdata);
}
