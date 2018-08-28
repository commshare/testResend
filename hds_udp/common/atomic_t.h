#ifndef _ATMOIC_T_H_
#define _ATMOIC_T_H_

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#endif


template <typename T> 
class atomic_t
{
private:
	volatile T val;

public:

	atomic_t(T c1)
	{
		val = c1;
	}

	atomic_t()
	{
		val =0;
	}

	inline void operator = (const T c2)
	{
#ifdef WIN32
		InterlockedExchange(reinterpret_cast<volatile long *>(&val),c2);
#else
        __sync_lock_test_and_set(&val,c2);
#endif
	}



	inline T AtomicExchange(const T t1)
	{
#ifdef WIN32
		return InterlockedExchange(reinterpret_cast<volatile long *>(&val),t1);
#else
        return __sync_lock_test_and_set(&val,t1);
#endif
	}
	

	inline bool operator==(const T t1) const
	{
		return val==t1;
	}

	inline bool operator!=(const T t1) const
	{
		return val!=t1;
	}

	inline T operator ++(int)
	{
#ifdef WIN32
		return InterlockedExchangeAdd(reinterpret_cast<volatile long *>(&val),1);
#else
                return __sync_fetch_and_add(&val,1);
#endif
	}

	inline T operator --(int)
	{
#ifdef WIN32      
		return InterlockedExchangeAdd(reinterpret_cast<volatile long *>(&val),-1);
#else
        return __sync_fetch_and_sub(&val,1);
#endif
	}

	inline int operator ++()
	{
#ifdef WIN32   
		return InterlockedIncrement(reinterpret_cast<volatile long *>(&val));
#else
		return __sync_add_and_fetch(&val,1);
#endif
	}

	inline int operator --()
	{
#ifdef WIN32 
		return InterlockedDecrement(reinterpret_cast<volatile long *>(&val));
#else
		return __sync_sub_and_fetch(&val,1);
#endif
	}

	inline bool operator<=(const T t1)
	{
		return val <= t1;
	}

	inline bool operator<(const T t1)
	{
		return val < t1;
	}

	inline bool operator >(const T t1)
	{
		return val > t1;
	}

	inline bool operator==(const T t1)
	{
		return val == t1;
	}

	inline bool operator>=(const T t1)
	{
		return val >= t1;
	}

	inline T add(const T t1)
	{
#ifdef WIN32
		return InterlockedExchangeAdd(reinterpret_cast<volatile long *>(&val),t1);
#else
        //return __sync_fetch_and_add(&val,t1);
		// 2014-3-03 Liao Jie 
		// 非Windows环境下也要对val进行reinterpret_cast转换，不然会报错
		return __sync_fetch_and_add(reinterpret_cast<volatile long *>(&val),t1);
#endif
	}

	inline T get_value()
	{
		return val;
	}

	inline T value()
	{
		return val;
	}

	inline T CompareExchange(const T set_value,const T comparend)
	{
#ifdef WIN32
		return InterlockedCompareExchange(reinterpret_cast<volatile long *>(&val),set_value,comparend);
#else
		return __sync_val_compare_and_swap(&val,comparend,set_value);
#endif
	}


	inline T AtomicBitTestAndSet(const long offset)
	{
#ifdef WIN32
		return InterlockedBitTestAndSet(reinterpret_cast<volatile long *>(&val),offset);
#else
        long offset_t = 1L<<offset;
        return (__sync_fetch_and_or(&val,offset_t)&offset_t)>>offset;
#endif
	}

	inline T AtomicBitTestAndReset(const long offset)
	{
#ifdef WIN32
		return InterlockedBitTestAndReset(reinterpret_cast<volatile long *>(&val),offset);
#else
        long offset_t = 1L<<offset;
        return (__sync_fetch_and_and(&val,~(offset_t))&offset_t)>>offset;
#endif
	}

//此函数只能在x86架构下运行
inline T AtomicBitTestAndComplement(const long offset)
	{
#ifdef WIN32   
		return InterlockedBitTestAndComplement(reinterpret_cast<volatile long *>(&val),offset); 
#else
		int old = 0;
		__asm__ __volatile__("lock ; btcl %2,%1\n\tsbbl %0,%0 "
			:"=r" (old),"=m" ((*(volatile long *) &val))
			:"Ir" (offset));
		return (old != 0);
#endif
	}



	inline T BitTest(const long offset)
	{
		return (this->val &  (1L<<offset))>0?1:0;
	}
};


template<class T>
class atomic_t<T*>
{
private:
	volatile T* val;

public:

	atomic_t(T* c1)
	{
		val = c1;
	}

	atomic_t()
	{
		val =0;
	}

	inline void operator = (const T* c2)
	{
#ifdef WIN32
		InterlockedExchange(reinterpret_cast<volatile long *>(&val),(unsigned long)c2);
#else
		__sync_lock_test_and_set(&val,(unsigned long)c2);
#endif
	}



	inline T* AtomicExchange(const T* t1)
	{
#ifdef WIN32
		return (T*)InterlockedExchange(reinterpret_cast<volatile long *>(&val), (unsigned long)t1);
#else
		return (T*)__sync_lock_test_and_set(&val,(unsigned long)t1);
#endif
	}


	inline bool operator==(const T* t1) const
	{
		return val==t1;
	}

	inline bool operator!=(const T* t1) const
	{
		return (unsigned long)val!=(unsigned long)t1;
	}

	inline T* operator ++(int)
	{
#ifdef WIN32
		return (T*)InterlockedExchangeAdd(reinterpret_cast<volatile long *>(&val),1);
#else
		return (T*)__sync_fetch_and_add(&val,1);
#endif
	}

	inline T* operator --(int)
	{
#ifdef WIN32      
		return (T*)InterlockedExchangeAdd(reinterpret_cast<volatile long *>(&val),-1);
#else
		return (T*)__sync_fetch_and_sub(&val,1);
#endif
	}

	inline int operator ++()
	{
#ifdef WIN32   
		return InterlockedIncrement(reinterpret_cast<volatile long *>(&val));
#else
		return __sync_add_and_fetch(&val,1);
#endif
	}

	inline int operator --()
	{
#ifdef WIN32 
		return InterlockedDecrement(reinterpret_cast<volatile long *>(&val));
#else
		return __sync_sub_and_fetch(&val,1);
#endif
	}

	inline bool operator<=(const T* t1)
	{
		return (unsigned long)val <= (unsigned long)t1;
	}

	inline bool operator>=(const T* t1)
	{
		return (unsigned long)val >= (unsigned long)t1;
	}

	inline T* add(const T* t1)
	{
#ifdef WIN32
		return (T*)InterlockedExchangeAdd(reinterpret_cast<volatile long *>(&val),(LONG)t1);
#else
		return (T*)__sync_fetch_and_add(&val,(unsigned long)t1);
#endif
	}

	inline T* get_value()
	{
		return val;
	}

	inline T* value()
	{
		return (T*)val;
	}

	inline T* CompareExchange(const T* set_value,const T* comparend)
	{
#ifdef WIN32
		return (T*)InterlockedCompareExchange(reinterpret_cast<volatile long *>(&val),(LONG)set_value,(LONG)comparend);
#else
		return (T*)__sync_val_compare_and_swap(&val,(volatile T*)comparend,(volatile T *)set_value);
#endif
	}


	inline T* AtomicBitTestAndSet(const long offset)
	{
#ifdef WIN32
		return (T*)InterlockedBitTestAndSet(reinterpret_cast<volatile long *>(&val),offset);
#else
		long offset_t = 1L<<offset;
		return (T*)((__sync_fetch_and_or(&val,offset_t)&offset_t)>>offset);
#endif
	}

	inline T* AtomicBitTestAndReset(const long offset)
	{
#ifdef WIN32
		return (T*)InterlockedBitTestAndReset(reinterpret_cast<volatile long *>(&val),offset);
#else
		long offset_t = 1L<<offset;
		return (T*)((__sync_fetch_and_and(&val,~(offset_t))&offset_t)>>offset);
#endif
	}

	//此函数只能在x86架构下运行
	inline T* AtomicBitTestAndComplement(const long offset)
	{
#ifdef WIN32   
		return (T*)InterlockedBitTestAndComplement(reinterpret_cast<volatile long *>(&val),offset); 
#else
		int old = 0;
		__asm__ __volatile__("lock ; btcl %2,%1\n\tsbbl %0,%0 "
			:"=r" (old),"=m" ((*(volatile long *) &val))
			:"Ir" (offset));
		return (old != 0);
#endif
	}



	inline T* BitTest(const long offset)
	{
		return (T*)((this->value &  (1L<<offset))>0?1:0);
	}
};
#endif
