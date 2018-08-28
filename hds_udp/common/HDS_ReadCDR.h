#ifndef _HDS_READER_CDR_H_
#define _HDS_READER_CDR_H_
/*HDS������ReadCDR
*/
/*��HDSReaderCDR�������ڴ�߽���
ʹ��ʱ��ע���ڴ�Խ������*/

/*
ע�⣬�����У���������64λ����32λ��ϵͳ�У�long��int������Ϊ4Byte
��Ϣ���в�����64λ������
����ֻ����x86�ܹ�������С���ֽ������������Ӳ���ܹ���������ʵ�ִ��ࡣ
*/
class HDS_ReaderCDR
{
private:
	char *current_pos;
	char *base_pos;

public:
	char * get_current_pos(){
		return current_pos;
	}

	HDS_ReaderCDR(char* data):current_pos(data),base_pos(data){}

	inline int length(){return current_pos-base_pos;}

	inline void read_ulong(unsigned int &x)
	{
		x=*(int*)current_pos; //htonl(*(long*)current_pos);
		current_pos+=4;
	}

	inline void read_long(int &x)
	{
		x= *(unsigned int*)current_pos;//htonl(*(unsigned int*)current_pos);
		current_pos+=4;
	}

	inline void read_ushort(unsigned short&x)
	{
		x= *(unsigned short*)current_pos;//htons(*(unsigned short*)current_pos);
		current_pos+=2;
	}

	inline void read_short(short&x)
	{
		x=*(short*)current_pos;// htons(*(short*)current_pos);
		current_pos+=2;
	}

	inline void read_char_array(char* x,int length)
	{
		memcpy(x,current_pos,length);
		current_pos+=length;
	}

	//void read_longlong(long long &x);

	inline void read_char(char &x)
	{
		x=*(char*)current_pos;
		current_pos++;
	}

	inline void skip_long(){
		this->current_pos+=4;
	}

	inline void skip_ulong(){
		this->current_pos+=4;
	}
};
#endif