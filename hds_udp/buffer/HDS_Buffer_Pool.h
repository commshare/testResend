#ifndef _HDS_BUFFER_POOL_H_
#define _HDS_BUFFER_POOL_H_
#include "../common/HDS_Mutex.h"

#define HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE 512
#define HDS_UDP_POOL_DEFAULT_ALLOC_BLOCK_ARRAY_SIZE 128

#define HDS_UDP_BUFFER_POOL_ARRAY_SIZE 64
#define HDS_UDP_BUFFER_POOL_CTRL_UNIT 8
#define HDS_BUFFER_POOL_MAX_CTL_SIZE (HDS_UDP_BUFFER_POOL_ARRAY_SIZE*HDS_UDP_BUFFER_POOL_CTRL_UNIT)

namespace HDS_UDP
{
	namespace Buffer_Pool
	{

		int init();

		void* get_buffer(const int size);

		void release_buffer(void * const ptr, const int size);

		int fini();
	}

	struct _pool_base_
	{	
		int _malloc_size;
		char * _head;
		char**  _buf_list;
		int _buf_list_offset;
		int _buf_list_size;

	public:
		void init(int object_size);

		char* get_object();

		void release_object(const char * buf);

		void destroy();
	};


	struct _mutex_pool_base_
	{	
		int _malloc_size;
		char * _head;
		char**  _buf_list;
		int _buf_list_offset;
		int _buf_list_size;
		HDS_Mutex _mutex;

	public:
		void init(int object_size);

		char* get_object();

		void release_object(const char * buf);

		void destroy();
	};

	template<int malloc_size>
	class HDS_Singleton_Pool
	{
	private:
		HDS_Singleton_Pool(){}
	public:
		 static void * malloc()
		 {
			 return Buffer_Pool::get_buffer(malloc_size);
		 }

		 static void free(void * const ptr)
		 {
			 Buffer_Pool::release_buffer(ptr,malloc_size);
		 }
	};
}


#endif