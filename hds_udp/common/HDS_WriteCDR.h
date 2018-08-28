#ifndef _HDS_WRITECDR_H_ 
#define _HDS_WRITECDR_H_
/*HDS的数据WriteCDR
*/
/*本HDSWriteCDR不进行内存边界检查
使用时请注意内存越界问题*/


/*
注意，本类中，无论是在64位还是32位的系统中，long和int都将视为4Byte
消息体中不会有64位的数据
由于只考虑x86架构，属于小端字节序，如果有其他硬件架构，请重新实现此类。
*/
class HDS_WriteCDR
{
private:
	char * current_pos;
	char*  base_pos;

public:
	HDS_WriteCDR(char *pos):current_pos(pos),base_pos(pos){}

	inline int length(){return current_pos-base_pos;}

	inline void write_long(int x)
	{
		*(int*)current_pos = x;//ntohl(x);
		current_pos+=4;
	}

	inline void write_ulong(unsigned int x)
	{
		*(unsigned int*)current_pos =x;// ntohl(x);
		current_pos+=4;
	}

	inline void write_short(short x)
	{
		*(short*)current_pos =x;// ntohs(x);
		current_pos+=2;
	}

	inline void write_ushort(unsigned short x)
	{
		*(unsigned short*)current_pos =x;// ntohs(x);
		current_pos+=2;
	}

	inline void write_char(char x)
	{
		*current_pos = x;
		current_pos++;
	}

	inline void write_char_array(char* x, unsigned int length)
	{
		memcpy(current_pos,x,length);
		current_pos+=length;
	}
};

#endif