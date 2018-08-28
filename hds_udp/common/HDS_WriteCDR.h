#ifndef _HDS_WRITECDR_H_ 
#define _HDS_WRITECDR_H_
/*HDS������WriteCDR
*/
/*��HDSWriteCDR�������ڴ�߽���
ʹ��ʱ��ע���ڴ�Խ������*/


/*
ע�⣬�����У���������64λ����32λ��ϵͳ�У�long��int������Ϊ4Byte
��Ϣ���в�����64λ������
����ֻ����x86�ܹ�������С���ֽ������������Ӳ���ܹ���������ʵ�ִ��ࡣ
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