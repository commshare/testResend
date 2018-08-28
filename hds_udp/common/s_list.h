#ifndef _S_LIST_H_
#define _S_LIST_H_
/*
Author:Sunning
单向链表，boost的pool
执行效率上比stlport的快3倍左右。
执行1000000次尾部插入头部删除：
s_list:  469ms
stlport::list: 1364ms;
本链表尾部进，头部出，测试时使用stlport中的是双向链表，这是stlport中的单链表只能提供头部进头部出的缘故。
内存消耗上基本差不多，但由于stlport不回收小块内存以免影响效率，所以在clear后stlport的内存占用较大
此单向链表在多线程下写是不安全的。
*/

#include <boost/pool/pool.hpp>

template <typename T>
class s_list
{
private:
	struct node
	{
		node * next;
		T   data;
	};

	boost::pool<boost::default_user_allocator_malloc_free> Pool_node;

	node* head;
	node* tail;
	unsigned int cur_size;

public:
	class iterator
	{
		friend class s_list<T>;
	private:
		node *current;
	public:
		iterator operator ++ (int){
			if(this->current !=NULL)
				this->current= current->next;
			return *this;
		}

		T value(){
			return current->data;
		}

		bool operator ==(const iterator& op)
		{
			return this->current == op.current;
		}

		bool operator !=(const iterator& op)
		{
			return this->current != op.current;
		}
	};

	iterator begin(){
		iterator xnode;
		xnode.current= head;
		return xnode;
	}

	iterator end(){
		iterator xnode;
		xnode.current= NULL;
		return xnode;
	}

	s_list():Pool_node(sizeof(node))
	{
		head=0;
		tail =0;
		cur_size =0;
	}
	~s_list()
	{
		clear();
	}

	inline int size()
	{
		return cur_size;
	}

	inline int push_back(const T& t1)
	{
		node* temp = reinterpret_cast<node*>(Pool_node.malloc());
		if(temp == 0)
			return -1;
		temp->next=NULL;
		temp->data= t1;
		if(cur_size == 0)
		{
			head = temp;
			tail = temp;
		}
		else
		{
			tail->next=temp;
			tail = temp;
		}
		cur_size++;
		return cur_size;
	}

	inline int dequeue_head(T&t1)
	{
		if(cur_size == 0)
			return -1;
		else if(cur_size == 1)
		{
			t1 = head->data;
			Pool_node.free(head);
			head = NULL;
			tail = NULL;
			cur_size--;
		}
		else
		{
			t1 = head->data;
			node * temp = head;
			head = head->next;
			cur_size--;
			Pool_node.free(temp);
		}
		return 0;
	}

	inline void clear()
	{
		cur_size = 0;
		head =0;
		tail =0;
		Pool_node.purge_memory();
	}

	inline int find(const T& t1)
	{
		node * temp = head;
		while(temp){
			if(temp->data == t1)
				return 0;
			temp = temp->next;
		}
		return -1;
	}

	int find_and_del(const T& t1)
	{
		node * temp = head;
		node * xtemp=0;
		while(temp){
			if(temp->data == t1)
			{
				if(temp == head){
					head= head->next;
					Pool_node.free(temp);
					cur_size--;
				}
				else{
					xtemp->next = temp->next;
					Pool_node.free(temp);
					cur_size--;
				}
				return 0;
			}
			xtemp= temp;
			temp = temp->next;
		}
		return -1;
	}
};


#endif