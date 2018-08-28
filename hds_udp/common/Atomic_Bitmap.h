#ifndef  _ATOMIC_BITMAP_H_
#define _ATOMIC_BITMAP_H_
#include "atomic_t.h"

const  long LONG_HAS_BIT=sizeof(long)*8;

#ifndef WIN32
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#endif

class Atomic_Bitmap
{
public:
	int m_count;
	//--- copy control functions -------------------------
	Atomic_Bitmap(int bitNum)
	{
		this->_bit_num = bitNum;
		this->_lsize = ((bitNum/LONG_HAS_BIT)+ ((bitNum-bitNum/LONG_HAS_BIT)>0?1:0));
		this->_pbits = (atomic_t<long> *)malloc(this->_lsize*sizeof(long));
		memset(this->_pbits,0,this->_lsize*sizeof(long));
	}

	Atomic_Bitmap(const Atomic_Bitmap * bm)
	{
		this->_bit_num = bm->_bit_num;
		this->_lsize = bm->_lsize;
		this->_pbits = (atomic_t<long> *)malloc(this->_lsize*sizeof(long));
		memcpy(this->_pbits,bm->_pbits,this->_lsize*sizeof(long));
	}

	~Atomic_Bitmap()
	{
		free(this->_pbits);
	}

	inline int set(int pos)
	{
		if(pos>=this->_bit_num)
			return -1;

		atomic_t<long> *intpos = (this->_pbits);
		intpos += pos/LONG_HAS_BIT;
		return intpos->AtomicBitTestAndSet(pos%LONG_HAS_BIT);
	}

	inline int reset(int pos)
	{
		if(pos>=this->_bit_num)
			return -1;

		atomic_t<long> *intpos = (this->_pbits);
		intpos += pos/LONG_HAS_BIT;
		return intpos->AtomicBitTestAndReset(pos%LONG_HAS_BIT);
	}
	// clear all the bits when tagClear is true(default); otherwise, set all the bits
	inline void clear(bool tagClear = true)
	{
		if(tagClear)
			memset(this->_pbits,0xff,this->_lsize*sizeof(long));
		else
			memset(this->_pbits,0,this->_lsize*sizeof(long));
	}

	// flip the bit at position pos
	//this function only works in x86 amd64 platform
	inline int flip(int pos)
	{
		if(pos>=this->_bit_num)
			return -1;

		atomic_t<long> *intpos = (this->_pbits);
		intpos += pos/LONG_HAS_BIT;
		return intpos->AtomicBitTestAndComplement(pos%LONG_HAS_BIT);
	}


	// test the bit at position pos
	inline bool test(int pos) const
	{
		if(pos>=this->_bit_num)
			return false;

		atomic_t<long> *intpos = (this->_pbits);
		intpos += pos/LONG_HAS_BIT;
		return (intpos->value() & (1L<<(pos%LONG_HAS_BIT)))>0?true:false;
	}

	// get the number of bits in this BitSet object
	inline int get_bit_size()
	{
		return this->_bit_num;
	}

	inline int get_char_size()
	{
		return this->_lsize*sizeof(long);
	}

	//查找到从start_pos开始（包括start_pos）第一个标记为1的位置，返回-1则失败
	int findFirstSetBit(int start_pos)
	{
		// function: test the bit at position pos. 
		// return: If the bit is set, then return true; otherwise return false.
		if (start_pos>=this->_bit_num)
			return -1;

		// compute the corresponding index of elements(ULONG) and remaining bits 
		long eleIndex = start_pos / LONG_HAS_BIT;
		long bitIndex = start_pos % LONG_HAS_BIT;

		while(true)
		{
			while(this->_pbits[eleIndex].value()==0 && eleIndex< this->_lsize)
			{
				bitIndex=0;
				eleIndex++;
			}

			if(eleIndex==this->_lsize)
				return -1;

			unsigned long mask = (unsigned long)(-1);
			mask <<=bitIndex;
			long val =this->_pbits[eleIndex].value() & mask;
			if(val == 0)
			{
				eleIndex++;
				bitIndex=0;
				continue;
			}

			long pos;
#ifdef WIN32
			__asm
			{
				mov eax,val;
				bsf ebx,eax;
				mov pos,ebx;
			}
#elif __LP64__
			asm("bsfq %1,%0\n\t"
				: "=r" (pos)
				: "r" (val));
#else
			 __asm__ ("bsf %1,%0":
			"=r" (pos):
			"r"  (val));
#endif
			return eleIndex*LONG_HAS_BIT+pos;
		}
	}

	int findFirstUnsetBit(int start_pos)
	{
		if (start_pos >= this->_bit_num)
			return -1;

		// compute the corresponding index of elements(ULONG) and remaining bits 
		long eleIndex = start_pos / LONG_HAS_BIT;
		long bitIndex = start_pos % LONG_HAS_BIT;

		while(true)
		{
			while(this->_pbits[eleIndex].value()==-1 && eleIndex<this->_lsize)
			{
				bitIndex=0;
				eleIndex++;
			}

			if(eleIndex==this->_lsize)
				return -1;

			unsigned long mask = (unsigned long)(-1);
			mask <<=bitIndex;
			long val =~(this->_pbits[eleIndex].value() | ~mask);
			if(val == 0)
			{
				eleIndex++;
				bitIndex=0;
				continue;
			}

			long pos;
#ifdef WIN32
			__asm
			{
				mov eax,val;
				bsf ebx,eax;
				mov pos,ebx;
			}
#elif __LP64__
			asm("bsfq %1,%0\n\t"
				: "=r" (pos)
				: "r" (val));
#else
			__asm__ ("bsf %1,%0":
			"=r" (pos):
			"r"  (val));
#endif
			return eleIndex*LONG_HAS_BIT+pos;
		}
	}

	int findFirstSetBit_R(int start_pos)
	{
		if (start_pos >= this->_bit_num)
			return -1;

		// compute the corresponding index of elements(ULONG) and remaining bits 
		long eleIndex = start_pos / LONG_HAS_BIT;
		long bitIndex = start_pos % LONG_HAS_BIT;

		while(true)
		{
			while(this->_pbits[eleIndex].value()==0 && eleIndex>=0)
			{
				bitIndex = LONG_HAS_BIT-1;
				eleIndex--;
			}

			if(eleIndex== -1)
				return -1;

			unsigned long mask =(unsigned long)(-1);
			mask = mask>>(LONG_HAS_BIT-bitIndex-1);
			long val =this->_pbits[eleIndex].value() & mask;
			if(val == 0)
			{
				eleIndex--;
				bitIndex = LONG_HAS_BIT-1;
				continue;
			}

			long pos;
#ifdef WIN32
			__asm
			{
				mov eax,val;
				bsr ebx,eax;
				mov pos,ebx;
			}
#elif __LP64__
			asm("bsrq %1,%0\n\t"
				: "=r" (pos)
				: "r" (val));
#else
			__asm__ ("bsr %1,%0":
			"=r" (pos):
			"r"  (val));
#endif
			return eleIndex*LONG_HAS_BIT+pos;
		}
	}

	int findFirstUnsetBit_R(int start_pos)
	{
		// function: test the bit at position pos. 
		// return: If the bit is set, then return true; otherwise return false.
		if (start_pos >= this->_bit_num)
			return -1;

		// compute the corresponding index of elements(ULONG) and remaining bits 
		long eleIndex = start_pos / LONG_HAS_BIT;
		long bitIndex = start_pos % LONG_HAS_BIT;

		while(true)
		{
			while(this->_pbits[eleIndex].value()==-1 && eleIndex>=0)
			{
				bitIndex = LONG_HAS_BIT-1;
				eleIndex--;
			}

			if(eleIndex== -1)
				return -1;

			unsigned long mask =(unsigned long)(-1);
			mask >>=(LONG_HAS_BIT-bitIndex-1);
			long val =~(this->_pbits[eleIndex].value() | ~mask);
			if(val == 0)
			{
				eleIndex--;
				bitIndex = LONG_HAS_BIT-1;
				continue;
			}

			long pos;
#ifdef WIN32
			__asm
			{
				mov eax,val;
				bsr ebx,eax;
				mov pos,ebx;
			}
#elif __LP64__
			asm("bsrq %1,%0\n\t"
				: "=r" (pos)
				: "r" (val));
#else
			__asm__ ("bsr %1,%0":
			"=r" (pos):
			"r"  (val));
#endif


			return eleIndex*LONG_HAS_BIT+pos;
		}
	}

private:
	int _bit_num;  // the number of bits in this BitSet object
	int _lsize;    // the number of elements with type BitSet::block_type used to store the bits
	atomic_t<long> *_pbits; // a pointer to an array with element type BitSet::block_type,
	// in the array element with the highest address
};


#endif