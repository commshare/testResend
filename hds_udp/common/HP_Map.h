#ifndef _HIGH_PERFORMANCE_MAP_H_
#define _HIGH_PERFORMANCE_MAP_H_

#include <boost/pool/pool.hpp>

/*
Author:sunning
Date:2011.2.22
此map按照linux内核红黑树的实现修改而成。
这是个速度很快的map，比stlport的map快4倍左右，最快时接近6倍
****************************************
测试结果：
map<int,int>
插入1000000个数据后，读取1000000次数据，再完全删除。
HP_map使用时间：3418ms
stlport   使用时间：16555ms
内存占用大致相同
由于stlport内部有内存池，所以clear内存不会马上被释放，hp_map可以完全释放掉。从内存占用上看，在某些方面HP_map比stlport有优势。
****************************************
使用boost_pool可能会使内存占用变大，但速度是一流的。
此map对于多线程读是安全的
如果多线程写需要加入同步机制。
HP_map与stl的map特性一致，插入相同key的数据，旧数据不会被覆盖。
*/

#define    RB_TREE_RED        0
#define    RB_TREE_BLACK    1


template <typename T1,typename T2>
class HP_map
{
private:
	/* 定义自己的entry，first是key，second是value。*/
	struct my_data{
		T1 first;
		T2 second;
	};

	struct rb_node {
		//在rb_node中添加自己的数据
		struct my_data key;
		struct rb_node *rb_parent;
		int rb_color;
		struct rb_node *rb_right;
		struct rb_node *rb_left;
		//used for select and rank function. when a new rb_node is initialized, weight should be 1.
		int weight;
	};

	struct rb_root {
		struct rb_node *rb_node;
		int size;
	};


	/*如果使用一个全局的内存分配池，则应该会更加的高效*/
	boost::pool<boost::default_user_allocator_malloc_free> Pool_node;

	rb_root  root;

	/*定义自己的数据的比较函数*/
	static int cmp(struct my_data * left, struct my_data * right){
		if(left->first == right->first)
			return 0;
		else if(left->first<right->first)
			return -1;
		else
			return 1;
	}

	static inline void __rb_rotate_left(struct rb_node *node, struct rb_root *root)
	{
		struct rb_node *right = node->rb_right;

		int a=0,b=0,c=0;
		if(node->rb_left)
			a=node->rb_left->weight;
		if(right->rb_left)
			b=right->rb_left->weight;
		if(right->rb_right)
			c=right->rb_right->weight;
		node->weight=a+b+1;
		right->weight=a+b+c+2;

		if ((node->rb_right = right->rb_left))
			right->rb_left->rb_parent = node;
		right->rb_left = node;

		if ((right->rb_parent = node->rb_parent))
		{
			if (node == node->rb_parent->rb_left)
				node->rb_parent->rb_left = right;
			else
				node->rb_parent->rb_right = right;
		}
		else
			root->rb_node = right;
		node->rb_parent = right;
	}

	static void __rb_rotate_right(struct rb_node *node, struct rb_root *root)
	{
		struct rb_node *left = node->rb_left;

		int a=0,b=0,c=0;
		if(node->rb_right)
			c=node->rb_right->weight;
		if(left->rb_left)
			a=left->rb_left->weight;
		if(left->rb_right)
			b=left->rb_right->weight;
		node->weight=b+c+1;
		left->weight=a+b+c+2;

		if ((node->rb_left = left->rb_right))
			left->rb_right->rb_parent = node;
		left->rb_right = node;

		if ((left->rb_parent = node->rb_parent))
		{
			if (node == node->rb_parent->rb_right)
				node->rb_parent->rb_right = left;
			else
				node->rb_parent->rb_left = left;
		}
		else
			root->rb_node = left;
		node->rb_parent = left;
	}

	static void rb_insert_color(struct rb_node *node, struct rb_root *root)
	{
		struct rb_node *parent, *gparent;

		while ((parent = node->rb_parent) && parent->rb_color == RB_TREE_RED)
		{
			gparent = parent->rb_parent;

			if (parent == gparent->rb_left)
			{
				{
					register struct rb_node *uncle = gparent->rb_right;
					if (uncle && uncle->rb_color == RB_TREE_RED)
					{
						uncle->rb_color = RB_TREE_BLACK;
						parent->rb_color = RB_TREE_BLACK;
						gparent->rb_color = RB_TREE_RED;
						node = gparent;
						continue;
					}
				}

				if (parent->rb_right == node)
				{
					register struct rb_node *tmp;
					__rb_rotate_left(parent, root);
					tmp = parent;
					parent = node;
					node = tmp;
				}

				parent->rb_color = RB_TREE_BLACK;
				gparent->rb_color = RB_TREE_RED;
				__rb_rotate_right(gparent, root);
			} else {
				{
					register struct rb_node *uncle = gparent->rb_left;
					if (uncle && uncle->rb_color == RB_TREE_RED)
					{
						uncle->rb_color = RB_TREE_BLACK;
						parent->rb_color = RB_TREE_BLACK;
						gparent->rb_color = RB_TREE_RED;
						node = gparent;
						continue;
					}
				}

				if (parent->rb_left == node)
				{
					register struct rb_node *tmp;
					__rb_rotate_right(parent, root);
					tmp = parent;
					parent = node;
					node = tmp;
				}

				parent->rb_color = RB_TREE_BLACK;
				gparent->rb_color = RB_TREE_RED;
				__rb_rotate_left(gparent, root);
			}
		}

		root->rb_node->rb_color = RB_TREE_BLACK;
	}

	static void __rb_erase_color(struct rb_node *node, struct rb_node *parent,
	struct rb_root *root)
	{
		struct rb_node *other;

		while ((!node || node->rb_color == RB_TREE_BLACK) && node != root->rb_node)
		{
			if (parent->rb_left == node)
			{
				other = parent->rb_right;
				if (other->rb_color == RB_TREE_RED)
				{
					other->rb_color = RB_TREE_BLACK;
					parent->rb_color = RB_TREE_RED;
					__rb_rotate_left(parent, root);
					other = parent->rb_right;
				}
				if ((!other->rb_left ||
					other->rb_left->rb_color == RB_TREE_BLACK)
					&& (!other->rb_right ||
					other->rb_right->rb_color == RB_TREE_BLACK))
				{
					other->rb_color = RB_TREE_RED;
					node = parent;
					parent = node->rb_parent;
				}
				else
				{
					if (!other->rb_right ||
						other->rb_right->rb_color == RB_TREE_BLACK)
					{
						register struct rb_node *o_left;
						if ((o_left = other->rb_left))
							o_left->rb_color = RB_TREE_BLACK;
						other->rb_color = RB_TREE_RED;
						__rb_rotate_right(other, root);
						other = parent->rb_right;
					}
					other->rb_color = parent->rb_color;
					parent->rb_color = RB_TREE_BLACK;
					if (other->rb_right)
						other->rb_right->rb_color = RB_TREE_BLACK;
					__rb_rotate_left(parent, root);
					node = root->rb_node;
					break;
				}
			}
			else
			{
				other = parent->rb_left;
				if (other->rb_color == RB_TREE_RED)
				{
					other->rb_color = RB_TREE_BLACK;
					parent->rb_color = RB_TREE_RED;
					__rb_rotate_right(parent, root);
					other = parent->rb_left;
				}
				if ((!other->rb_left ||
					other->rb_left->rb_color == RB_TREE_BLACK)
					&& (!other->rb_right ||
					other->rb_right->rb_color == RB_TREE_BLACK))
				{
					other->rb_color = RB_TREE_RED;
					node = parent;
					parent = node->rb_parent;
				}
				else
				{
					if (!other->rb_left ||
						other->rb_left->rb_color == RB_TREE_BLACK)
					{
						register struct rb_node *o_right;
						if ((o_right = other->rb_right))
							o_right->rb_color = RB_TREE_BLACK;
						other->rb_color = RB_TREE_RED;
						__rb_rotate_left(other, root);
						other = parent->rb_left;
					}
					other->rb_color = parent->rb_color;
					parent->rb_color = RB_TREE_BLACK;
					if (other->rb_left)
						other->rb_left->rb_color = RB_TREE_BLACK;
					__rb_rotate_right(parent, root);
					node = root->rb_node;
					break;
				}
			}
		}
		if (node)
			node->rb_color = RB_TREE_BLACK;
	}

	void rb_erase(struct rb_node *node, struct rb_root *root)
	{
		root->size--;
		struct rb_node *child, *parent;
		int color;

		if (!node->rb_left)
			child = node->rb_right;
		else if (!node->rb_right)
			child = node->rb_left;
		else
		{
			struct rb_node *old = node, *left;

			node = node->rb_right;
			while ((left = node->rb_left) != NULL)
				node = left;
			child = node->rb_right;
			parent = node->rb_parent;
			color = node->rb_color;

			if (child)
				child->rb_parent = parent;
			if (parent)
			{
				if (parent->rb_left == node)
					parent->rb_left = child;
				else
					parent->rb_right = child;
			}
			else
				root->rb_node = child;

			if (node->rb_parent == old)
				parent = node;

			struct rb_node * q = parent;
			while(q){
				q->weight--;
				q=q->rb_parent;
			}

			node->rb_parent = old->rb_parent;
			node->rb_color = old->rb_color;
			node->weight = old->weight;
			node->rb_right = old->rb_right;
			node->rb_left = old->rb_left;

			if (old->rb_parent)
			{
				if (old->rb_parent->rb_left == old)
					old->rb_parent->rb_left = node;
				else
					old->rb_parent->rb_right = node;
			} else
				root->rb_node = node;

			old->rb_left->rb_parent = node;
			if (old->rb_right)
				old->rb_right->rb_parent = node;
			goto color;
		}

		parent = node->rb_parent;
		color = node->rb_color;

		if (child)
			child->rb_parent = parent;
		if (parent)
		{
			struct rb_node * q = parent;
			while(q){
				q->weight--;
				q=q->rb_parent;
			}
			if (parent->rb_left == node)
				parent->rb_left = child;
			else
				parent->rb_right = child;
		}
		else
			root->rb_node = child;

color:
		if (color == RB_TREE_BLACK)
			__rb_erase_color(child, parent, root);
	}

	/*
	* This function returns the first node (in sort order) of the tree.
	*/
	static struct rb_node *rb_first(struct rb_root *root)
	{
		struct rb_node    *n;

		n = root->rb_node;
		if (!n)
			return NULL;
		while (n->rb_left)
			n = n->rb_left;
		return n;
	}

	static struct rb_node *rb_last(struct rb_root *root)
	{
		struct rb_node    *n;

		n = root->rb_node;
		if (!n)
			return NULL;
		while (n->rb_right)
			n = n->rb_right;
		return n;
	}

	static struct rb_node *rb_next(struct rb_node *node)
	{
		/* If we have a right-hand child, go down and then left as far
		as we can. */
		if (node->rb_right) {
			node = node->rb_right; 
			while (node->rb_left)
				node=node->rb_left;
			return node;
		}

		/* No right-hand children. Everything down and left is
		smaller than us, so any 'next' node must be in the general
		direction of our parent. Go up the tree; any time the
		ancestor is a right-hand child of its parent, keep going
		up. First time it's a left-hand child of its parent, said
		parent is our 'next' node. */
		while (node->rb_parent && node == node->rb_parent->rb_right)
			node = node->rb_parent;

		return node->rb_parent;
	}

	static struct rb_node *rb_prev(struct rb_node *node)
	{
		/* If we have a left-hand child, go down and then right as far
		as we can. */
		if (node->rb_left) {
			node = node->rb_left; 
			while (node->rb_right)
				node=node->rb_right;
			return node;
		}

		/* No left-hand children. Go up till we find an ancestor which
		is a right-hand child of its parent */
		while (node->rb_parent && node == node->rb_parent->rb_left)
			node = node->rb_parent;

		return node->rb_parent;
	}

	static int my_rb_insert(struct rb_node *t, struct rb_root *root)
	{
		struct rb_node **p = &root->rb_node;
		struct rb_node *parent = NULL;
		int sig;

		while (*p) {
			parent = *p;
			parent->weight++;
			sig = cmp(&t->key, &parent->key);
			if (sig < 0)
				p = &(parent)->rb_left;
			else if (sig > 0)
				p = &(parent)->rb_right;
			else{
				while(parent){
					parent->weight--;
					parent=parent->rb_parent;
				}
				return -1;
			}
		}
		root->size++;
		t->rb_parent = parent;
		t->rb_color = RB_TREE_RED;
		t->rb_left = t->rb_right = NULL;
		t->weight=1;
		*p = t;
		rb_insert_color(t, root);
		return 0;
	}

	static struct rb_node * my_rb_find(struct my_data * key, struct rb_root *root)
	{
		struct rb_node *n = root->rb_node;
		int sig;

		while (n) {
			sig = cmp(key, &n->key);
			if (sig < 0)
				n = n->rb_left;
			else if (sig > 0)
				n = n->rb_right;
			else
				return n;
		}
		return NULL;
	}
	/*order base is from 1 to root->size*/
	static struct rb_node * select(int order, struct rb_root * root){
		if(order<=0 || order > root->size)
			return NULL;
		struct rb_node * cn=root->rb_node;
		int ls=0;
		while(cn){
			if(cn->rb_left)
				ls=cn->rb_left->weight;
			else ls=0;
			if(ls+1==order)
				return cn;
			else if(ls+1>order){
				cn=cn->rb_left;
			}else{
				order-=ls+1;
				cn=cn->rb_right;
			}
		}
		return NULL;
	}
	/*return a integer which is the number of rb_node->key <= w->key */
	static int rank(struct my_data * w, struct rb_root * root){
		if(!root->rb_node)
			return 0;
		int res=0;
		struct rb_node * cn=root->rb_node;
		int sig;
		while(cn)
		{
			if(&cn->key.first ==w->first)
			{
				if(sig==0)
				{
					return res+1+(cn->rb_left?cn->rb_left->weight:0);
				}
				else if(&cn->key.first <w->first)
				{
					res+=1+(cn->rb_left?cn->rb_left->weight:0);
					cn = cn->rb_right;
				}
				else
				{
					cn = cn->rb_left;
				}
			}
		}
		return res;
	}

public:
	HP_map():Pool_node(sizeof(rb_node))
	{
		root.rb_node=0;
		root.size=0;
	}

	~HP_map()
	{
		Pool_node.purge_memory();
	}


	inline int  insert(const T1 &first, const T2 &second)
	{
		rb_node * node = (rb_node*)Pool_node.malloc();
		if(node == 0)
			return -1;
		node->key.first=first;
		node->key.second=second;
		return my_rb_insert(node,&root);
	}



	inline int find(const T1& first, T2&second)
	{
		my_data fdata;
		fdata.first=first;
		rb_node* fnode = NULL;
		fnode = my_rb_find(&fdata,&root);
		if(fnode)
		{
			second = fnode->key.second;
			return 0;
		}
		return -1;
	}

	inline int find(const T1& first)
	{
		my_data fdata;
		fdata.first=first;
		rb_node* fnode = NULL;
		fnode = my_rb_find(&fdata,&root);
		if(fnode)
		{
			return 0;
		}
		return -1;
	}

	inline int find_and_del(const T1& first)
	{
		my_data fdata;
		fdata.first=first;
		rb_node* fnode = NULL;
		fnode = my_rb_find(&fdata,&root);
		if(fnode)
		{
			rb_erase(fnode,&root);
			Pool_node.free(fnode);
			return 0;
		}
		return -1;
	}

	inline int find_and_del(const T1& first,T2 &second)
	{
		my_data fdata;
		fdata.first=first;
		rb_node* fnode = NULL;
		fnode = my_rb_find(&fdata,&root);
		if(fnode)
		{
			second = fnode->key.second;
			rb_erase(fnode,&root);
			Pool_node.free(fnode);
			return 0;
		}
		return -1;
	}

	inline int erase(const T1&first)
	{
		my_data fdata;
		fdata.first=first;
		rb_node* fnode = NULL;
		fnode = my_rb_find(&fdata,&root);
		if(fnode)
		{
			rb_erase(fnode,&root);
			Pool_node.free(fnode);
			return 0;
		}
		return -1;
	}

	inline int change(const T1& first,const  T2 & second)
	{
		my_data fdata;
		fdata.first=first;
		rb_node* fnode = NULL;
		fnode = my_rb_find(&fdata,&root);
		if(fnode)
		{
			fnode->key.second = second;
			return 0;
		}
		return -1;
	}

	inline void clear()
	{
		root.rb_node=0;
		root.size=0;
		Pool_node.purge_memory();
	}

	inline int size()
	{
		return root.size;
	}

	class iterator
	{
		friend class HP_map<T1,T2>;
	private:
		rb_node* current_node;
		rb_root root;

	public:
		T1 first(){
			return current_node->key.first;
		}

		T2 second(){
			return current_node->key.second;
		}

		bool operator ==(const iterator& op)
		{
			return this->current_node == op.current_node;
		}

		bool operator !=(const iterator& op)
		{
			return this->current_node != op.current_node;
		}

		iterator operator ++ (int){
			if(this->current_node !=NULL)
				this->current_node= rb_next(this->current_node);
			return *this;
		}

		iterator operator -- (int){
			if(this->current_node !=NULL)
				this->current_node= rb_prev(this->current_node);
			else
				this->current_node = rb_last(&this->root);
			return *this;
		}
	};

	inline iterator begin(){
		iterator xnode;
		xnode.current_node= rb_first(&this->root);
		xnode.root =  this->root;
		return xnode;
	}

	iterator end(){
		iterator xnode;
		xnode.current_node= NULL;
		xnode.root =  this->root;
		return xnode;
	}
};


#endif