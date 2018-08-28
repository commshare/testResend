#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include "HDS_Buffer_Pool.h"
#include "../common/HDS_Mutex.h"
#include <stdlib.h>
#include <string.h>

namespace HDS_UDP
{
	void _pool_base_::init(int object_size)
	{
		_buf_list_offset=0;
		_buf_list_size = HDS_UDP_POOL_DEFAULT_ALLOC_BLOCK_ARRAY_SIZE;
		_buf_list = (char**)malloc(_buf_list_size* sizeof(char*));
		memset(_buf_list,0,_buf_list_size* sizeof(char*));
		_malloc_size = object_size;
		_head = (char*)malloc(HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE* _malloc_size);
		_buf_list[_buf_list_offset++] = _head;
		for(int i=0;i<HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE-1;i++)
		{
			*((char **)(&_head[i*_malloc_size])) = _head + (i+1)*_malloc_size;
		}
		*((char **)(&_head[(HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE-1)*_malloc_size]))=NULL; 
	}

	char* _pool_base_::get_object()
	{
		char * rebuf=NULL;
		if(_head)
		{
			rebuf = _head;
			_head =*((char**)&_head[0]);
			return rebuf;
		}
		else
		{
			if(_buf_list_offset == _buf_list_size)
			{
				char** nlist = (char**)malloc((_buf_list_size<<1)*sizeof(char*));
				if(nlist == NULL)
				{
					return NULL;
				}
				memset(nlist,0,(_buf_list_size<<1)*sizeof(char*));
				memcpy(nlist,_buf_list,_buf_list_size*sizeof(char*));
				free(_buf_list);
				_buf_list = nlist;
				_buf_list_size = _buf_list_size<<1;
			}
			char* nnode = (char*)malloc(HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE* _malloc_size);
			if(nnode == NULL)
			{
				return NULL;
			}
			_buf_list[_buf_list_offset++] = nnode;
			for(int i=1;i<HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE-1;i++)
			{
				*((char **)(&nnode[i*_malloc_size])) = nnode + (i+1)*_malloc_size;
			}
			*((char **)(&nnode[(HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE-1)*_malloc_size]))=_head;
			_head = nnode + _malloc_size;
			rebuf = nnode;
		}
		return rebuf;
	}

	void _pool_base_::release_object(const char * buf)
	{
		const char* tmp = buf;
		*((char**)&buf[0])=_head;
		_head=(char*)tmp;
	}

	void _pool_base_::destroy()
	{
		for(int i=0;i<this->_buf_list_offset;i++)
		{
			if(this->_buf_list[i]!= NULL)
			{
				free(this->_buf_list[i]);
			}
			else
			{
				break;
			}
		}
		free(_buf_list);
	}


	void _mutex_pool_base_::init(int object_size)
	{
		HDS_Mutex_Init(&this->_mutex);
		this->_buf_list_offset=0;
		this->_buf_list_size = HDS_UDP_POOL_DEFAULT_ALLOC_BLOCK_ARRAY_SIZE;
		this->_buf_list = (char**)malloc(_buf_list_size* sizeof(char*));
		memset(_buf_list,0,_buf_list_size* sizeof(char*));
		this->_malloc_size = object_size;
		this->_head = (char*)malloc(HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE* _malloc_size);
		this->_buf_list[_buf_list_offset++] = _head;
		for(int i=0;i<HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE-1;i++)
		{
			*((char **)(&this->_head[i*_malloc_size])) = this->_head + (i+1)*_malloc_size;
		}
		*((char **)(&this->_head[(HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE-1)*_malloc_size]))=NULL; 
	}

	char* _mutex_pool_base_::get_object()
	{
		char * rebuf=NULL;
		HDS_Mutex_Lock(&this->_mutex);
		if(this->_head)
		{
			rebuf = _head;
			this->_head =*((char**)&this->_head[0]);
			goto get_succ;
		}
		else
		{
			if(this->_buf_list_offset == this->_buf_list_size)
			{
				char** nlist = (char**)malloc((this->_buf_list_size<<1)*sizeof(char*));
				if(nlist == NULL)
				{
					goto get_succ;
				}
				memset(nlist,0,(this->_buf_list_size<<1)*sizeof(char*));
				memcpy(nlist,_buf_list,_buf_list_size*sizeof(char*));
				free(this->_buf_list);
				this->_buf_list = nlist;
				this->_buf_list_size = this->_buf_list_size<<1;
			}
			char* nnode = (char*)malloc(HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE* this->_malloc_size);
			if(nnode == NULL)
			{
				goto get_succ;
			}
			this->_buf_list[_buf_list_offset++] = nnode;
			for(int i=1;i<HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE-1;i++)
			{
				*((char **)(&nnode[i*this->_malloc_size])) = nnode + (i+1)*this->_malloc_size;
			}
			*((char **)(&nnode[(HDS_UDP_POOL_DEFAULT_ALLOC_OBJECT_SIZE-1)*this->_malloc_size]))=this->_head;
			this->_head = nnode + this->_malloc_size;
			rebuf = nnode;
			goto get_succ;
		}
get_succ:
		HDS_Mutex_Unlock(&this->_mutex);
		return rebuf;
	}

	void _mutex_pool_base_::release_object(const char * buf)
	{
		const char* tmp = buf;
		HDS_Mutex_Lock(&this->_mutex);
		*((char**)&buf[0])=_head;
		_head=(char*)tmp;
		HDS_Mutex_Unlock(&this->_mutex);
	}

	void _mutex_pool_base_::destroy()
	{
		for(int i=0;i<this->_buf_list_offset;i++)
		{
			if(this->_buf_list[i] != NULL)
			{
				free(this->_buf_list[i]);
			}
			else
			{
				break;
			}
		}
		free(_buf_list);
		HDS_Mutex_Delete(&this->_mutex);
	}

	namespace Buffer_Pool
	{
		static _mutex_pool_base_ _pool_base_array[HDS_UDP_BUFFER_POOL_ARRAY_SIZE];

		int init()
		{
			memset(_pool_base_array,0,HDS_UDP_BUFFER_POOL_ARRAY_SIZE*sizeof(_pool_base_));
			return 0;
		}

		void* get_buffer(const int size)
		{
			void* rebuf=NULL;
			int offset = (size-1)/HDS_UDP_BUFFER_POOL_CTRL_UNIT;
			if(offset<HDS_UDP_BUFFER_POOL_ARRAY_SIZE)
			{
				if(_pool_base_array[offset]._buf_list == 0)
				{
					_pool_base_array[offset].init((offset+1)*HDS_UDP_BUFFER_POOL_CTRL_UNIT);
					rebuf = (void*)_pool_base_array[offset].get_object();
				}
				else
				{
					rebuf = (void*)_pool_base_array[offset].get_object();
				}
			}
			else
			{
				rebuf = malloc(size);
			}
			return rebuf;
		}

		void release_buffer(void * const ptr, const int size)
		{
			int offset = (size-1)/HDS_UDP_BUFFER_POOL_CTRL_UNIT;
			if(offset < HDS_UDP_BUFFER_POOL_ARRAY_SIZE)
			{
				_pool_base_array[offset].release_object((char*)ptr);
			}
			else
			{
				free(ptr);
			}
		}

		int fini()
		{
			for(int i=0;i<HDS_UDP_BUFFER_POOL_ARRAY_SIZE;i++)
			{
				_pool_base_array[i].destroy();
			}
			return 0;
		}
	}
}