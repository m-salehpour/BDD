/*
 
Implemented by Masoud (Pouya) Salehpour based on: https://github.com/begeekmyfriend/bplustree

Original License:

The MIT License (MIT)

Copyright (c) 2014 Leo Ma

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

>>>>>>> Please visit https://github.com/begeekmyfriend/bplustree for more information
 */



#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>       // for clock_t, clock(), CLOCKS_PER_SEC
//*************************************************
//can be taken to be inserted in a .h file as well

#define BPLUS_MIN_ORDER     3
#define BPLUS_MAX_ORDER     64
#define BPLUS_MAX_ENTRIES   64
#define BPLUS_MAX_LEVEL     10
#define BPLUS_MAX_REPEAT    100 
#define N_DIGITS_P            2 //placeholder for predicates
#define st_size 2500

typedef int key_t;
typedef char key_s[st_size];

struct list_head {
        struct list_head *prev, *next;
};

static inline void list_init(struct list_head *link)
{
        link->prev = link;
        link->next = link;
}

static inline void __list_add(struct list_head *link, struct list_head *prev, struct list_head *next)
{
        link->next = next;
        link->prev = prev;
        next->prev = link;
        prev->next = link;
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
        prev->next = next;
        next->prev = prev;
}

static inline void list_add(struct list_head *link, struct list_head *prev)
{
        __list_add(link, prev, prev->next);
}

static inline void list_add_tail(struct list_head *link, struct list_head *head)
{
	__list_add(link, head->prev, head);
}

static inline void list_del(struct list_head *link)
{
        __list_del(link->prev, link->next);
        list_init(link);
}

static inline int list_is_first(struct list_head *link, struct list_head *head)
{
	return link->prev == head;
}

static inline int list_is_last(struct list_head *link, struct list_head *head)
{
	return link->next == head;
}

#define list_entry(ptr, type, member) \
        ((type *)((char *)(ptr) - (size_t)(&((type *)0)->member)))

#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_prev_entry(pos, member) \
	list_entry((pos)->member.prev, typeof(*(pos)), member)

struct bplus_node {
        int type;
        int parent_key_idx;
        struct bplus_non_leaf *parent;
        struct list_head link;
        int count;
};

struct bplus_non_leaf {
        int type;
        int parent_key_idx;
        struct bplus_non_leaf *parent;
        struct list_head link;
        int children;
        int key[BPLUS_MAX_ORDER - 1];
        struct bplus_node *sub_ptr[BPLUS_MAX_ORDER];
};

struct bplus_leaf {
        int type;
        int parent_key_idx;
        struct bplus_non_leaf *parent;
        struct list_head link;
        int entries;
        int key[BPLUS_MAX_ENTRIES];
        int data[BPLUS_MAX_ENTRIES][BPLUS_MAX_REPEAT];
};

struct bplus_tree {
        int order;
        int entries;
        int level;
        struct bplus_node *root;
        struct list_head list[BPLUS_MAX_LEVEL];
};

void bplus_tree_dump(struct bplus_tree *tree);
int bplus_tree_get(struct bplus_tree *tree, key_t key);
int bplus_tree_put(struct bplus_tree *tree, key_t key, int data);
int bplus_tree_get_range(struct bplus_tree *tree, key_t key1, key_t key2);
struct bplus_tree *bplus_tree_init(int order, int entries);
void bplus_tree_deinit(struct bplus_tree *tree);


//**************************************************

enum {
        INVALID_OFFSET = 0xdeadbeef,
};

enum {
        BPLUS_TREE_LEAF,
        BPLUS_TREE_NON_LEAF = 1,
};

enum {
        LEFT_SIBLING,
        RIGHT_SIBLING = 1,
};

#define ADDR_STR_WIDTH 16
#define offset_ptr(node) ((char *) (node) + sizeof(*node))
#define key(node) ((key_t *)offset_ptr(node))
#define data(node) ((long *)(offset_ptr(node) + _max_entries * sizeof(key_t)))
#define sub(node) ((off_t *)(offset_ptr(node) + (_max_order - 1) * sizeof(key_t)))

static int _block_size;
static int _max_entries;
static int _max_order;

static inline int is_leaf(struct bplus_node *node)
{
        return node->type == BPLUS_TREE_LEAF;
}




static key_t key_binary_search(key_t *arr, int len, key_t target)
{
        int low = -1;
        int high = len;
        while (low + 1 < high) {
                int mid = low + (high - low) / 2;
                if (target > arr[mid]) {
                        low = mid;
                } else {
                        high = mid;
                }
        }
        if (high >= len || arr[high] != target) {
                return -high - 1;
        } else {
                return high;
        }
}

static struct bplus_non_leaf *non_leaf_new(void)
{
        struct bplus_non_leaf *node = calloc(1, sizeof(*node));
        assert(node != NULL);
        list_init(&node->link);
        node->type = BPLUS_TREE_NON_LEAF;
        node->parent_key_idx = -1;
        return node;
}

static struct bplus_leaf *leaf_new(void)
{
        struct bplus_leaf *node = calloc(1, sizeof(*node));
        assert(node != NULL);
        list_init(&node->link);
        node->type = BPLUS_TREE_LEAF;
        node->parent_key_idx = -1;
        return node;
}

static void non_leaf_delete(struct bplus_non_leaf *node)
{
        list_del(&node->link);
        free(node);
}

static void leaf_delete(struct bplus_leaf *node)
{
        list_del(&node->link);
        free(node);
}









static int bplus_tree_search(struct bplus_tree *tree, key_t key)
{
        int i, ret = -1;

	bool multi_SP=false;
        struct bplus_node *node = tree->root;
        while (node != NULL) {
                if (is_leaf(node)) {
                        struct bplus_leaf *ln = (struct bplus_leaf *)node;
                        i = key_binary_search(ln->key, ln->entries, key);

                        //ret = i >= 0 ? ln->data[i] : 0;
		
	
			/*
			if (key == 29810 )
			{
				getchar();

				printf("Bingo!!");


			}
			*/

			if (i >= 0)
			{
				for (int k=0; k<BPLUS_MAX_REPEAT;k++)
				{
					printf("\n %d:  key:%d   [value]:%d\n",k+1, key, ln->data[i][k]);
					if(ln->data[i][k]==0)
					{
						ret = ln->data[i][k-1];
						break;
					}
					
					/*
					if (k >= 30)
						multi_SP = true;
					*/

				}
				printf("\n*******************************\n");

				/*
				if (multi_SP)
					getchar();
				*/
				

			}else
			{
				ret = 0;
			}
			
			
                        break;
                } else {
                        struct bplus_non_leaf *nln = (struct bplus_non_leaf *)node;
                        i = key_binary_search(nln->key, nln->children - 1, key);
                        if (i >= 0) {
                                node = nln->sub_ptr[i + 1];
                        } else {
                                i = -i - 1;
                                node = nln->sub_ptr[i];
                        }
                }
        }
        return ret;
}

static int non_leaf_insert(struct bplus_tree *tree, struct bplus_non_leaf *node,
                           struct bplus_node *l_ch, struct bplus_node *r_ch, key_t key, int level);

static int parent_node_build(struct bplus_tree *tree, struct bplus_node *left,
                             struct bplus_node *right, key_t key, int level)
{
        if (left->parent == NULL && right->parent == NULL) {
                /* new parent */
                struct bplus_non_leaf *parent = non_leaf_new();
                parent->key[0] = key;
                parent->sub_ptr[0] = left;
                parent->sub_ptr[0]->parent = parent;
                parent->sub_ptr[0]->parent_key_idx = -1;
                parent->sub_ptr[1] = right;
                parent->sub_ptr[1]->parent = parent;
                parent->sub_ptr[1]->parent_key_idx = 0;
                parent->children = 2;
                /* update root */
                tree->root = (struct bplus_node *)parent;
                list_add(&parent->link, &tree->list[++tree->level]);
                return 0;
        } else if (right->parent == NULL) {
                /* trace upwards */
                right->parent = left->parent;
                return non_leaf_insert(tree, left->parent, left, right, key, level + 1);
        } else {
                /* trace upwards */
                left->parent = right->parent;
                return non_leaf_insert(tree, right->parent, left, right, key, level + 1);
        }
}

static int non_leaf_split_left(struct bplus_non_leaf *node, struct bplus_non_leaf *left,
                               struct bplus_node *l_ch, struct bplus_node *r_ch, key_t key, int insert)
{
        int i, j, order = node->children;
        key_t split_key;
        /* split = [m/2] */
        int split = (order + 1) / 2;
        /* split as left sibling */
        __list_add(&left->link, node->link.prev, &node->link);
        /* replicate from sub[0] to sub[split - 1] */
        for (i = 0, j = 0; i < split; i++, j++) {
                if (j == insert) {
                        left->sub_ptr[j] = l_ch;
                        left->sub_ptr[j]->parent = left;
                        left->sub_ptr[j]->parent_key_idx = j - 1;
                        left->sub_ptr[j + 1] = r_ch;
                        left->sub_ptr[j + 1]->parent = left;
                        left->sub_ptr[j + 1]->parent_key_idx = j;
                        j++;
                } else {
                        left->sub_ptr[j] = node->sub_ptr[i];
                        left->sub_ptr[j]->parent = left;
                        left->sub_ptr[j]->parent_key_idx = j - 1;
                }
        }
        left->children = split;
        /* replicate from key[0] to key[split - 2] */
        for (i = 0, j = 0; i < split - 1; j++) {
                if (j == insert) {
                        left->key[j] = key;
                } else {
                        left->key[j] = node->key[i];
                        i++;
                }
        }
        if (insert == split - 1) {
                left->key[insert] = key;
                left->sub_ptr[insert] = l_ch;
                left->sub_ptr[insert]->parent = left;
                left->sub_ptr[insert]->parent_key_idx = j - 1;
                node->sub_ptr[0] = r_ch;
                split_key = key;
        } else {
                node->sub_ptr[0] = node->sub_ptr[split - 1];
                split_key = node->key[split - 2];
        }
        node->sub_ptr[0]->parent = node;
        node->sub_ptr[0]->parent_key_idx = - 1;
        /* left shift for right node from split - 1 to children - 1 */
        for (i = split - 1, j = 0; i < order - 1; i++, j++) {
                node->key[j] = node->key[i];
                node->sub_ptr[j + 1] = node->sub_ptr[i + 1];
                node->sub_ptr[j + 1]->parent = node;
                node->sub_ptr[j + 1]->parent_key_idx = j;
        }
        node->sub_ptr[j] = node->sub_ptr[i];
        node->children = j + 1;
        return split_key;
}

static int non_leaf_split_right1(struct bplus_non_leaf *node, struct bplus_non_leaf *right,
                                 struct bplus_node *l_ch, struct bplus_node *r_ch, key_t key, int insert)
{
        int i, j, order = node->children;
        key_t split_key;
        /* split = [m/2] */
        int split = (order + 1) / 2;
        /* split as right sibling */
        list_add(&right->link, &node->link);
        /* split key is key[split - 1] */
        split_key = node->key[split - 1];
        /* left node's children always be [split] */
        node->children = split;
        /* right node's first sub-node */
        right->key[0] = key;
        right->sub_ptr[0] = l_ch;
        right->sub_ptr[0]->parent = right;
        right->sub_ptr[0]->parent_key_idx = -1;
        right->sub_ptr[1] = r_ch;
        right->sub_ptr[1]->parent = right;
        right->sub_ptr[1]->parent_key_idx = 0;
        /* insertion point is split point, replicate from key[split] */
        for (i = split, j = 1; i < order - 1; i++, j++) {
                right->key[j] = node->key[i];
                right->sub_ptr[j + 1] = node->sub_ptr[i + 1];
                right->sub_ptr[j + 1]->parent = right;
                right->sub_ptr[j + 1]->parent_key_idx = j;
        }
        right->children = j + 1;
        return split_key;
}

static int non_leaf_split_right2(struct bplus_non_leaf *node, struct bplus_non_leaf *right,
                                 struct bplus_node *l_ch, struct bplus_node *r_ch, key_t key, int insert)
{
        int i, j, order = node->children;
        key_t split_key;
        /* split = [m/2] */
        int split = (order + 1) / 2;
        /* left node's children always be [split + 1] */
        node->children = split + 1;
        /* split as right sibling */
        list_add(&right->link, &node->link);
        /* split key is key[split] */
        split_key = node->key[split];
        /* right node's first sub-node */
        right->sub_ptr[0] = node->sub_ptr[split + 1];
        right->sub_ptr[0]->parent = right;
        right->sub_ptr[0]->parent_key_idx = -1;
        /* replicate from key[split + 1] to key[order - 1] */
        for (i = split + 1, j = 0; i < order - 1; j++) {
                if (j != insert - split - 1) {
                        right->key[j] = node->key[i];
                        right->sub_ptr[j + 1] = node->sub_ptr[i + 1];
                        right->sub_ptr[j + 1]->parent = right;
                        right->sub_ptr[j + 1]->parent_key_idx = j;
                        i++;
                }
        }
        /* reserve a hole for insertion */
        if (j > insert - split - 1) {
                right->children = j + 1;
        } else {
                assert(j == insert - split - 1);
                right->children = j + 2;
        }
        /* insert new key and sub-node */
        j = insert - split - 1;
        right->key[j] = key;
        right->sub_ptr[j] = l_ch;
        right->sub_ptr[j]->parent = right;
        right->sub_ptr[j]->parent_key_idx = j - 1;
        right->sub_ptr[j + 1] = r_ch;
        right->sub_ptr[j + 1]->parent = right;
        right->sub_ptr[j + 1]->parent_key_idx = j;
        return split_key;
}

static void non_leaf_simple_insert(struct bplus_non_leaf *node, struct bplus_node *l_ch,
                                   struct bplus_node *r_ch, key_t key, int insert)
{
        int i;
        for (i = node->children - 1; i > insert; i--) {
                node->key[i] = node->key[i - 1];
                node->sub_ptr[i + 1] = node->sub_ptr[i];
                node->sub_ptr[i + 1]->parent_key_idx = i;
        }
        node->key[i] = key;
        node->sub_ptr[i] = l_ch;
        node->sub_ptr[i]->parent_key_idx = i - 1;
        node->sub_ptr[i + 1] = r_ch;
        node->sub_ptr[i + 1]->parent_key_idx = i;
        node->children++;
}

static int non_leaf_insert(struct bplus_tree *tree, struct bplus_non_leaf *node,
                           struct bplus_node *l_ch, struct bplus_node *r_ch, key_t key, int level)
{
        /* search key location */
        int insert = key_binary_search(node->key, node->children - 1, key);
        assert(insert < 0);
        insert = -insert - 1;

        /* node is full */
        if (node->children == tree->order) {
                /* split = [m/2] */
                key_t split_key;
                int split = (node->children + 1) / 2;
                struct bplus_non_leaf *sibling = non_leaf_new();
                if (insert < split) {
                        split_key = non_leaf_split_left(node, sibling, l_ch, r_ch, key, insert);
                } else if (insert == split) {
                        split_key = non_leaf_split_right1(node, sibling, l_ch, r_ch, key, insert);
                } else {
                        split_key = non_leaf_split_right2(node, sibling, l_ch, r_ch, key, insert);
                }
                /* build new parent */
                if (insert < split) {
                        return parent_node_build(tree, (struct bplus_node *)sibling,
                                (struct bplus_node *)node, split_key, level);
                } else {
                        return parent_node_build(tree, (struct bplus_node *)node,
                                (struct bplus_node *)sibling, split_key, level);
                }
        } else {
                non_leaf_simple_insert(node, l_ch, r_ch, key, insert);
        }

        return 0;
}

static void leaf_split_left(struct bplus_leaf *leaf, struct bplus_leaf *left,
                            key_t key, int data, int insert)
{
        int i, j;
        /* split = [m/2] */
        int split = (leaf->entries + 1) / 2;
        /* split as left sibling */
        __list_add(&left->link, leaf->link.prev, &leaf->link);
        /* replicate from 0 to key[split - 2] */
        for (i = 0, j = 0; i < split - 1; j++) {
                if (j == insert) {
                        left->key[j] = key;
			for (int k=0;k<BPLUS_MAX_REPEAT;k++)
	       		{     
		    		if (left->data[j][k] == 0)
	       			{        
		       			left->data[j][k] = data;
	       				left->data[j][k+1] = 0; 
	       				break;       
		      		}     
		    	}    

                       // left->data[j] = data;
                } else {
                        left->key[j] = leaf->key[i];
                        for (int k=0;k<BPLUS_MAX_REPEAT;k++)         
                        {                 
                                //if (left->data[j][k] == 0)
                                //{                         
                                        left->data[j][k] = leaf->data[i][k];
                                        //left->data[j][k+1] = 0;
                                       // break;
                               // }
                        }



                        //left->data[j] = leaf->data[i];
                        i++;
                }
        }
        if (j == insert) {
                left->key[j] = key;
		  for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
                                if (left->data[j][k] == 0)
                                {
                                        left->data[j][k] = data;
                                        left->data[j][k+1] = 0;
                                        break;
                                }
                        }


                //left->data[j] = data;
                j++;
        }
        left->entries = j;
        /* left shift for right node */
        for (j = 0; i < leaf->entries; i++, j++) {
                leaf->key[j] = leaf->key[i];

		  for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
                                //if (leaf->data[j][k] == 0)
                                //{
                                        leaf->data[j][k] = leaf->data[i][k];
                                    //    leaf->data[j][k+1] = 0;
                                  //      break;
                               // }
                        }

                //leaf->data[j] = leaf->data[i];
        }
        leaf->entries = j;
}

static void leaf_split_right(struct bplus_leaf *leaf, struct bplus_leaf *right,
                             key_t key, int data, int insert)
{
        int i, j;
        /* split = [m/2] */
        int split = (leaf->entries + 1) / 2;
        /* split as right sibling */
        list_add(&right->link, &leaf->link);
        /* replicate from key[split] */
        for (i = split, j = 0; i < leaf->entries; j++) {
                if (j != insert - split) {
                        right->key[j] = leaf->key[i];
			for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
                               // if (right->data[j][k] == 0)
                                //{
                                        right->data[j][k] = leaf->data[i][k];
                                    //    right->data[j][k+1] = 0;
                                  //      break;
                               // }
                        }

                        //right->data[j] = leaf->data[i];
                        i++;
                }
        }
        /* reserve a hole for insertion */
        if (j > insert - split) {
                right->entries = j;
        } else {
                assert(j == insert - split);
                right->entries = j + 1;
        }
        /* insert new key */
        j = insert - split;
        right->key[j] = key;
                        for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
                                if (right->data[j][k] == 0)
                                {
                                        right->data[j][k] = data;
                                        right->data[j][k+1] = 0;
                                        break;
                                }
                        }

   

        //right->data[j] = data;
        /* left leaf number */
        leaf->entries = split;
}












static void leaf_simple_insert(struct bplus_leaf *leaf, key_t key, int data, int insert)
{
	bool flag = false;
	if ( key_binary_search(leaf->key, leaf->entries, key) >= 0  )
		flag = true;

	int i;
	if (!flag)
	{
		
		for (i = leaf->entries; i > insert; i--) 
		{
			leaf->key[i] = leaf->key[i - 1];

			for (int k=0;k<BPLUS_MAX_REPEAT;k++)
			{
				leaf->data[i][k] = leaf->data[i - 1][k];
			}
		}
		leaf->key[i] = key;
		leaf->data[i][0] = data;
		leaf->data[i][1] = 0;
		leaf->entries++;
	}
	else
	{
      		for (i = leaf->entries; i > insert; i--)
                {
                        //leaf->key[i] = leaf->key[i - 1];
                        //leaf->data[i] = leaf->data[i - 1];
                }

		for (int k=0;k<BPLUS_MAX_REPEAT;k++)
		{
			if (leaf->data[i][k] == 0)
			{
				leaf->data[i][k] = data;
				leaf->data[i][k+1] = 0;
				break;
			}
		}





	}

}









static int leaf_insert(struct bplus_tree *tree, struct bplus_leaf *leaf, key_t key, int data)
{
        /* search key location */
        int insert = key_binary_search(leaf->key, leaf->entries, key);
        if (insert >= 0) {
                /* Already exists */
                //return -1;
		leaf_simple_insert(leaf, key, data, insert);
		return 0;
        }
	
		insert = -insert - 1;

        /* node full */
        if (leaf->entries == tree->entries   ) {
                /* split = [m/2] */
                int split = (tree->entries + 1) / 2;
                /* splited sibling node */
                struct bplus_leaf *sibling = leaf_new();
                /* sibling leaf replication due to location of insertion */
                if (insert < split) {
                        leaf_split_left(leaf, sibling, key, data, insert);
                } else {
                        leaf_split_right(leaf, sibling, key, data, insert);
                }
                /* build new parent */
                if (insert < split) {
                        return parent_node_build(tree, (struct bplus_node *)sibling,
                                (struct bplus_node *)leaf, leaf->key[0], 0);
                } else {
                        return parent_node_build(tree, (struct bplus_node *)leaf,
                                (struct bplus_node *)sibling, sibling->key[0], 0);
                }
        } else {
                leaf_simple_insert(leaf, key, data, insert);
        }

        return 0;
}






static int bplus_tree_insert(struct bplus_tree *tree, key_t key, int data)
{
        struct bplus_node *node = tree->root;
        while (node != NULL) {
                if (is_leaf(node)) {
                        struct bplus_leaf *ln = (struct bplus_leaf *)node;
                        return leaf_insert(tree, ln, key, data);
                } else {
                        struct bplus_non_leaf *nln = (struct bplus_non_leaf *)node;
                        int i = key_binary_search(nln->key, nln->children - 1, key);
                        if (i >= 0) {
                                node = nln->sub_ptr[i + 1];
                        } else {
                                i = -i - 1;
                                node = nln->sub_ptr[i];
                        }
                }
        }

        /* new root */
        struct bplus_leaf *root = leaf_new();
        root->key[0] = key;
        root->data[0][0] = data;
	root->data[0][1] = 0;//added---
        root->entries = 1;
        tree->root = (struct bplus_node *)root;
        list_add(&root->link, &tree->list[tree->level]);
        return 0;
}




static int non_leaf_sibling_select(struct bplus_non_leaf *l_sib, struct bplus_non_leaf *r_sib,
                                   struct bplus_non_leaf *parent, int i)
{
        if (i == -1) {
                /* the frist sub-node, no left sibling, choose the right one */
                return RIGHT_SIBLING;
        } else if (i == parent->children - 2) {
                /* the last sub-node, no right sibling, choose the left one */
                return LEFT_SIBLING;
        } else {
                /* if both left and right sibling found, choose the one with more entries */
                return l_sib->children >= r_sib->children ? LEFT_SIBLING : RIGHT_SIBLING;
        }
}

static void non_leaf_shift_from_left(struct bplus_non_leaf *node, struct bplus_non_leaf *left,
                                     int parent_key_index, int remove)
{
        int i;
        /* node's elements right shift */
        for (i = remove; i > 0; i--) {
                node->key[i] = node->key[i - 1];
        }
        for (i = remove + 1; i > 0; i--) {
                node->sub_ptr[i] = node->sub_ptr[i - 1];
                node->sub_ptr[i]->parent_key_idx = i - 1;
        }
        /* parent key right rotation */
        node->key[0] = node->parent->key[parent_key_index];
        node->parent->key[parent_key_index] = left->key[left->children - 2];
        /* borrow the last sub-node from left sibling */
        node->sub_ptr[0] = left->sub_ptr[left->children - 1];
        node->sub_ptr[0]->parent = node;
        node->sub_ptr[0]->parent_key_idx = -1;
        left->children--;
}

static void non_leaf_merge_into_left(struct bplus_non_leaf *node, struct bplus_non_leaf *left,
                                     int parent_key_index, int remove)
{
        int i, j;
        /* move parent key down */
        left->key[left->children - 1] = node->parent->key[parent_key_index];
        /* merge into left sibling */
        for (i = left->children, j = 0; j < node->children - 1; j++) {
                if (j != remove) {
                        left->key[i] = node->key[j];
                        i++;
                }
        }
        for (i = left->children, j = 0; j < node->children; j++) {
                if (j != remove + 1) {
                        left->sub_ptr[i] = node->sub_ptr[j];
                        left->sub_ptr[i]->parent = left;
                        left->sub_ptr[i]->parent_key_idx = i - 1;
                        i++;
                }
        }
        left->children = i;
        /* delete empty node */
        non_leaf_delete(node);
}

static void non_leaf_shift_from_right(struct bplus_non_leaf *node, struct bplus_non_leaf *right,
                                      int parent_key_index)
{
        int i;
        /* parent key left rotation */
        node->key[node->children - 1] = node->parent->key[parent_key_index];
        node->parent->key[parent_key_index] = right->key[0];
        /* borrow the frist sub-node from right sibling */
        node->sub_ptr[node->children] = right->sub_ptr[0];
        node->sub_ptr[node->children]->parent = node;
        node->sub_ptr[node->children]->parent_key_idx = node->children - 1;
        node->children++;
        /* left shift in right sibling */
        for (i = 0; i < right->children - 2; i++) {
                right->key[i] = right->key[i + 1];
        }
        for (i = 0; i < right->children - 1; i++) {
                right->sub_ptr[i] = right->sub_ptr[i + 1];
                right->sub_ptr[i]->parent_key_idx = i - 1;
        }
        right->children--;
}

static void non_leaf_merge_from_right(struct bplus_non_leaf *node, struct bplus_non_leaf *right,
                                      int parent_key_index)
{
        int i, j;
        /* move parent key down */
        node->key[node->children - 1] = node->parent->key[parent_key_index];
        node->children++;
        /* merge from right sibling */
        for (i = node->children - 1, j = 0; j < right->children - 1; i++, j++) {
                node->key[i] = right->key[j];
        }
        for (i = node->children - 1, j = 0; j < right->children; i++, j++) {
                node->sub_ptr[i] = right->sub_ptr[j];
                node->sub_ptr[i]->parent = node;
                node->sub_ptr[i]->parent_key_idx = i - 1;
        }
        node->children = i;
        /* delete empty right sibling */
        non_leaf_delete(right);
}

static void non_leaf_simple_remove(struct bplus_non_leaf *node, int remove)
{
        assert(node->children >= 2);
        for (; remove < node->children - 2; remove++) {
                node->key[remove] = node->key[remove + 1];
                node->sub_ptr[remove + 1] = node->sub_ptr[remove + 2];
                node->sub_ptr[remove + 1]->parent_key_idx = remove;
        }
        node->children--;
}

static void non_leaf_remove(struct bplus_tree *tree, struct bplus_non_leaf *node, int remove)
{
        if (node->children <= (tree->order + 1) / 2) {
                struct bplus_non_leaf *l_sib = list_prev_entry(node, link);
                struct bplus_non_leaf *r_sib = list_next_entry(node, link);
                struct bplus_non_leaf *parent = node->parent;
                if (parent != NULL) {
                        /* decide which sibling to be borrowed from */
                        int i = node->parent_key_idx;
                        if (non_leaf_sibling_select(l_sib, r_sib, parent, i) == LEFT_SIBLING) {
                                if (l_sib->children > (tree->order + 1) / 2) {
                                        non_leaf_shift_from_left(node, l_sib, i, remove);
                                } else {
                                        non_leaf_merge_into_left(node, l_sib, i, remove);
                                        /* trace upwards */
                                        non_leaf_remove(tree, parent, i);
                                }
                        } else {
                                /* remove first in case of overflow during merging with sibling */
                                non_leaf_simple_remove(node, remove);
                                if (r_sib->children > (tree->order + 1) / 2) {
                                        non_leaf_shift_from_right(node, r_sib, i + 1);
                                } else {
                                        non_leaf_merge_from_right(node, r_sib, i + 1);
                                        /* trace upwards */
                                        non_leaf_remove(tree, parent, i + 1);
                                }
                        }
                } else {
                        if (node->children == 2) {
                                /* delete old root node */
                                assert(remove == 0);
                                node->sub_ptr[0]->parent = NULL;
                                tree->root = node->sub_ptr[0];
                                non_leaf_delete(node);
                                tree->level--;
                        } else {
                                non_leaf_simple_remove(node, remove);
                        }
                }
        } else {
                non_leaf_simple_remove(node, remove);
        }
}

static int leaf_sibling_select(struct bplus_leaf *l_sib, struct bplus_leaf *r_sib,
                               struct bplus_non_leaf *parent, int i)
{
        if (i == -1) {
                /* the frist sub-node, no left sibling, choose the right one */
                return RIGHT_SIBLING;
        } else if (i == parent->children - 2) {
                /* the last sub-node, no right sibling, choose the left one */
                return LEFT_SIBLING;
        } else {
                /* if both left and right sibling found, choose the one with more entries */
                return l_sib->entries >= r_sib->entries ? LEFT_SIBLING : RIGHT_SIBLING;
        }
}

static void leaf_shift_from_left(struct bplus_leaf *leaf, struct bplus_leaf *left,
                                 int parent_key_index, int remove)
{
        /* right shift in leaf node */
        for (; remove > 0; remove--) {
                leaf->key[remove] = leaf->key[remove - 1];
                      for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
				leaf->data[remove][k] = leaf->data[remove - 1][k];
                                

                                //if (leaf->data[remove - 1][k] == 0)
                                  //      break;


                        }




                //leaf->data[remove] = leaf->data[remove - 1];
        }
        /* borrow the last element from left sibling */
        leaf->key[0] = left->key[left->entries - 1];

                      for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
				leaf->data[0][k] = left->data[left->entries - 1][k];

				//if (left->data[left->entries - 1][k] == 0)
                                  //      break;


                        }


        //leaf->data[0] = left->data[left->entries - 1];
        left->entries--;
        /* update parent key */
        leaf->parent->key[parent_key_index] = leaf->key[0];
}

static void leaf_merge_into_left(struct bplus_leaf *leaf, struct bplus_leaf *left, int remove)
{
        int i, j;
        /* merge into left sibling */
        for (i = left->entries, j = 0; j < leaf->entries; j++) {
                if (j != remove) {

			left->key[i] = leaf->key[j];

		       	for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
				left->data[i][k] = leaf->data[j][k];
                                //leaf->data[remove][k] = leaf->data[remove - 1][k];


                               // if (leaf->data[j][k] == 0)
                                 //       break;


                        }


                       // left->data[i] = leaf->data[j];
		       
                        i++;
                }
        }
        left->entries = i;
        /* delete merged leaf */
        leaf_delete(leaf);
}

static void leaf_shift_from_right(struct bplus_leaf *leaf, struct bplus_leaf *right, int parent_key_index)
{
        int i;
        /* borrow the first element from right sibling */
        leaf->key[leaf->entries] = right->key[0];

     for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
				leaf->data[leaf->entries][k] = right->data[0][k];

                                //leaf->data[i][k] = leaf->data[j][k];
                                //leaf->data[remove][k] = leaf->data[remove - 1][k];


                               // if (right->data[0][k] == 0)
                                 //       break;


                        }

        //leaf->data[leaf->entries] = right->data[0];
        leaf->entries++;
        /* left shift in right sibling */
        for (i = 0; i < right->entries - 1; i++) {
                right->key[i] = right->key[i + 1];

	       	for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
				right->data[i][k] = right->data[i + 1][k];
                                //left->data[i][k] = leaf->data[j][k];
                                //leaf->data[remove][k] = leaf->data[remove - 1][k];


                              //  if (right->data[i + 1][k] == 0)
                                 //       break;


                        }


               // right->data[i] = right->data[i + 1];
        }
        right->entries--;
        /* update parent key */
        leaf->parent->key[parent_key_index] = right->key[0];
}

static void leaf_merge_from_right(struct bplus_leaf *leaf, struct bplus_leaf *right)
{
        int i, j;
        /* merge from right sibling */
        for (i = leaf->entries, j = 0; j < right->entries; i++, j++) {
                leaf->key[i] = right->key[j];

	       	for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
                                leaf->data[i][k] = right->data[j][k];
                                //leaf->data[remove][k] = leaf->data[remove - 1][k];


                               // if (leaf->data[j][k] == 0)
                                 //       break;


                        }


               // leaf->data[i] = right->data[j];
        }
        leaf->entries = i;
        /* delete right sibling */
        leaf_delete(right);
}

static void leaf_simple_remove(struct bplus_leaf *leaf, int remove)
{
        for (; remove < leaf->entries - 1; remove++) {
                leaf->key[remove] = leaf->key[remove + 1];

	       	for (int k=0;k<BPLUS_MAX_REPEAT;k++)
                        {
				leaf->data[remove][k] = leaf->data[remove + 1][k];
                                //left->data[i][k] = leaf->data[j][k];
                                //leaf->data[remove][k] = leaf->data[remove - 1][k];


                                //if ( leaf->data[remove + 1][k] == 0)
                                  //      break;


                        }



               // leaf->data[remove] = leaf->data[remove + 1];
        }
        leaf->entries--;
}

static int leaf_remove(struct bplus_tree *tree, struct bplus_leaf *leaf, key_t key)
{
        int remove = key_binary_search(leaf->key, leaf->entries, key);
        if (remove < 0) {
                /* Not exist */
                return -1;
        }

        if (leaf->entries <= (tree->entries + 1) / 2) {
                struct bplus_non_leaf *parent = leaf->parent;
                struct bplus_leaf *l_sib = list_prev_entry(leaf, link);
                struct bplus_leaf *r_sib = list_next_entry(leaf, link);
                if (parent != NULL) {
                        /* decide which sibling to be borrowed from */
                        int i = leaf->parent_key_idx;
                        if (leaf_sibling_select(l_sib, r_sib, parent, i) == LEFT_SIBLING) {
                                if (l_sib->entries > (tree->entries + 1) / 2) {
                                        leaf_shift_from_left(leaf, l_sib, i, remove);
                                } else {
                                        leaf_merge_into_left(leaf, l_sib, remove);
                                        /* trace upwards */
                                        non_leaf_remove(tree, parent, i);
                                }
                        } else {
                                /* remove first in case of overflow during merging with sibling */
                                leaf_simple_remove(leaf, remove);
                                if (r_sib->entries > (tree->entries + 1) / 2) {
                                        leaf_shift_from_right(leaf, r_sib, i + 1);
                                } else {
                                        leaf_merge_from_right(leaf, r_sib);
                                        /* trace upwards */
                                        non_leaf_remove(tree, parent, i + 1);
                                }
                        }
                } else {
                        if (leaf->entries == 1) {
                                /* delete the only last node */
                                assert(key == leaf->key[0]);
                                tree->root = NULL;
                                leaf_delete(leaf);
                                return 0;
                        } else {
                                leaf_simple_remove(leaf, remove);
                        }
                }
        } else {
                leaf_simple_remove(leaf, remove);
        }

        return 0;
}

static int bplus_tree_delete(struct bplus_tree *tree, key_t key)
{
        struct bplus_node *node = tree->root;
        while (node != NULL) {
                if (is_leaf(node)) {
                        struct bplus_leaf *ln = (struct bplus_leaf *)node;
                        return leaf_remove(tree, ln, key);
                } else {
                        struct bplus_non_leaf *nln = (struct bplus_non_leaf *)node;
                        int i = key_binary_search(nln->key, nln->children - 1, key);
                        if (i >= 0) {
                                node = nln->sub_ptr[i + 1];
                        } else {
                                i = -i - 1;
                                node = nln->sub_ptr[i];
                        }
                }
        }
        return -1;
}

int bplus_tree_get(struct bplus_tree *tree, key_t key)
{
        int data = bplus_tree_search(tree, key); 
        if (data) {
                return data;
        } else {
                return -1;
        }
}


void bplus_tree_deinit(struct bplus_tree *tree)
{
        free(tree);
}

int bplus_tree_get_range(struct bplus_tree *tree, key_t key1, key_t key2)
{
    int i, data = 0;
    key_t min = key1 <= key2 ? key1 : key2;
    key_t max = min == key1 ? key2 : key1;
    struct bplus_node *node = tree->root;

    while (node != NULL) {
            if (is_leaf(node)) {
                    struct bplus_leaf *ln = (struct bplus_leaf *)node;
                    i = key_binary_search(ln->key, ln->entries, min);
                    if (i < 0) {
                            i = -i - 1;
                            if (i >= ln->entries) {
                                    if (list_is_last(&ln->link, &tree->list[0])) {
                                            return -1;
                                    }
                                    ln = list_next_entry(ln, link);
                            }
                    }
                    while (ln->key[i] <= max) {
                            data = ln->data[i];
                            if (++i >= ln->entries) {
                                    if (list_is_last(&ln->link, &tree->list[0])) {
                                            return -1;
                                    }
                                    ln = list_next_entry(ln, link);
                                    i = 0;
                            }
                    }
                    break;
            } else {
                    struct bplus_non_leaf *nln = (struct bplus_non_leaf *)node;
                    i = key_binary_search(nln->key, nln->children - 1, min);
                    if (i >= 0) {
                            node = nln->sub_ptr[i + 1];
                    } else  {
                            i = -i - 1;
                            node = nln->sub_ptr[i];
                    }
            }
    }

    return data;
}

#ifdef _BPLUS_TREE_DEBUG
struct node_backlog {
        /* Node backlogged */
        struct bplus_node *node;
        /* The index next to the backtrack point, must be >= 1 */
        int next_sub_idx;
};

static inline int children(struct bplus_node *node)
{
        return ((struct bplus_non_leaf *) node)->children;
}

static void node_key_dump(struct bplus_node *node)
{
        int i;
        if (is_leaf(node)) {
                for (i = 0; i < node->count; i++) {
                        printf("%d ", ((struct bplus_leaf *)node)->key[i]);
                }
        } else {
                for (i = 0; i < node->count - 1; i++) {
                        printf("%d ", ((struct bplus_non_leaf *)node)->key[i]);
                }
        }
        printf("\n");
}

static key_t node_key(struct bplus_node *node, int i)
{
        if (is_leaf(node)) {
                return ((struct bplus_leaf *)node)->key[i];
        } else {
                return ((struct bplus_non_leaf *)node)->key[i];
        }
}

static void key_print(struct bplus_node *node)
{
        int i;
        if (is_leaf(node)) {
                struct bplus_leaf *leaf = (struct bplus_leaf *)node;
                printf("leaf:");
                for (i = 0; i < leaf->entries; i++) {
                        printf(" %d", leaf->key[i]);
                }
        } else {
                struct bplus_non_leaf *non_leaf = (struct bplus_non_leaf *)node;
                printf("node:");
                for (i = 0; i < non_leaf->children - 1; i++) {
                        printf(" %d", non_leaf->key[i]);
                }
        }
        printf("\n");
}

void bplus_tree_dump(struct bplus_tree *tree)
{
        int level = 0;
        struct bplus_node *node = tree->root;
        struct node_backlog *p_nbl = NULL;
        struct node_backlog nbl_stack[BPLUS_MAX_LEVEL];
        struct node_backlog *top = nbl_stack;

        for (; ;) {
                if (node != NULL) {
                        /* non-zero needs backward and zero does not */
                        int sub_idx = p_nbl != NULL ? p_nbl->next_sub_idx : 0;
                        /* Reset each loop */
                        p_nbl = NULL;

                        /* Backlog the path */
                        if (is_leaf(node) || sub_idx + 1 >= children(node)) {
                                top->node = NULL;
                                top->next_sub_idx = 0;
                        } else {
                                top->node = node;
                                top->next_sub_idx = sub_idx + 1;
                        }
                        top++;
                        level++;

                        /* Draw the whole node when the first entry is passed through */
                        if (sub_idx == 0) {
                                int i;
                                for (i = 1; i < level; i++) {
                                        if (i == level - 1) {
                                                printf("%-8s", "+-------");
                                        } else {
                                                if (nbl_stack[i - 1].node != NULL) {
                                                        printf("%-8s", "|");
                                                } else {
                                                        printf("%-8s", " ");
                                                }
                                        }
                                }
                                key_print(node);
                        }

                        /* Move deep down */
                        node = is_leaf(node) ? NULL : ((struct bplus_non_leaf *) node)->sub_ptr[sub_idx];
                } else {
                        p_nbl = top == nbl_stack ? NULL : --top;
                        if (p_nbl == NULL) {
                                /* End of traversal */
                                break;
                        }
                        node = p_nbl->node;
                        level--;
                }
        }
}
#endif





int bplus_tree_put(struct bplus_tree *tree, key_t key, int data)
{
        if (data) {
                return bplus_tree_insert(tree, key, data);
        } else {
                return bplus_tree_delete(tree, key);
        }
}









struct bplus_tree *bplus_tree_init(int order, int entries)
{
        /* The max order of non leaf nodes must be more than two */
        assert(BPLUS_MAX_ORDER > BPLUS_MIN_ORDER);
        assert(order <= BPLUS_MAX_ORDER && entries <= BPLUS_MAX_ENTRIES);

        int i;
        struct bplus_tree *tree = calloc(1, sizeof(*tree));
        if (tree != NULL) {
                tree->root = NULL;
                tree->order = order;
                tree->entries = entries;
                for (i = 0; i < BPLUS_MAX_LEVEL; i++) {
                        list_init(&tree->list[i]);
                }
        }

        return tree;
}







//#ifdef _BPLUS_TREE_DEBUG
struct node_backlog {
        /* Node backlogged */
        struct bplus_node *node;
        /* The index next to the backtrack point, must be >= 1 */
        int next_sub_idx;
};

static inline int children(struct bplus_node *node)
{
        return ((struct bplus_non_leaf *) node)->children;
}

static void node_key_dump(struct bplus_node *node)
{
        int i;
        if (is_leaf(node)) {
                for (i = 0; i < node->count; i++) {
                        printf("%d ", ((struct bplus_leaf *)node)->key[i]);
                }
        } else {
                for (i = 0; i < node->count - 1; i++) {
                        printf("%d ", ((struct bplus_non_leaf *)node)->key[i]);
                }
        }
        printf("\n");
}

static key_t node_key(struct bplus_node *node, int i)
{
        if (is_leaf(node)) {
                return ((struct bplus_leaf *)node)->key[i];
        } else {
                return ((struct bplus_non_leaf *)node)->key[i];
        }
}

static void key_print(struct bplus_node *node)
{
        int i;
        if (is_leaf(node)) {
                struct bplus_leaf *leaf = (struct bplus_leaf *)node;
                printf("leaf:");
                for (i = 0; i < leaf->entries; i++) {
                        printf(" %d", leaf->key[i]);
                }
        } else {
                struct bplus_non_leaf *non_leaf = (struct bplus_non_leaf *)node;
                printf("node:");
                for (i = 0; i < non_leaf->children - 1; i++) {
                        printf(" %d", non_leaf->key[i]);
                }
        }
        printf("\n");
}

void bplus_tree_dump(struct bplus_tree *tree)
{
        int level = 0;
        struct bplus_node *node = tree->root;
        struct node_backlog *p_nbl = NULL;
        struct node_backlog nbl_stack[BPLUS_MAX_LEVEL];
        struct node_backlog *top = nbl_stack;

        for (; ;) {
                if (node != NULL) {
                        /* non-zero needs backward and zero does not */
                        int sub_idx = p_nbl != NULL ? p_nbl->next_sub_idx : 0;
                        /* Reset each loop */
                        p_nbl = NULL;

                        /* Backlog the path */
                        if (is_leaf(node) || sub_idx + 1 >= children(node)) {
                                top->node = NULL;
                                top->next_sub_idx = 0;
                        } else {
                                top->node = node;
                                top->next_sub_idx = sub_idx + 1;
                        }
                        top++;
                        level++;

                        /* Draw the whole node when the first entry is passed through */
                        if (sub_idx == 0) {
                                int i;
                                for (i = 1; i < level; i++) {
                                        if (i == level - 1) {
                                                printf("%-8s", "+-------");
                                        } else {
                                                if (nbl_stack[i - 1].node != NULL) {
                                                        printf("%-8s", "|");
                                                } else {
                                                        printf("%-8s", " ");
                                                }
                                        }
                                }
                                key_print(node);
                        }

                        /* Move deep down */
                        node = is_leaf(node) ? NULL : ((struct bplus_non_leaf *) node)->sub_ptr[sub_idx];
                } else {
                        p_nbl = top == nbl_stack ? NULL : --top;
                        if (p_nbl == NULL) {
                                /* End of traversal */
                                break;
                        }
                        node = p_nbl->node;
                        level--;
                }
        }
}
//#endif



//***********************************************************************************
// Test
//


struct bplus_tree_config {
        int order;
        int entries;
};

static void bplus_tree_get_put_test(struct bplus_tree *tree)
{
        int i;

        fprintf(stderr, "\n> B+tree getter and setter testing...\n");

        bplus_tree_put(tree, 24, 24);
        bplus_tree_put(tree, 72, 72);
        bplus_tree_put(tree, 1, 1);
        bplus_tree_put(tree, 39, 39);
        bplus_tree_put(tree, 53, 53);
        bplus_tree_put(tree, 63, 63);
        bplus_tree_put(tree, 90, 90);
        bplus_tree_put(tree, 88, 88);
        bplus_tree_put(tree, 15, 15);
        bplus_tree_put(tree, 10, 10);
        bplus_tree_put(tree, 44, 44);
        bplus_tree_put(tree, 68, 68);
        bplus_tree_put(tree, 74, 74);
        bplus_tree_dump(tree);

        bplus_tree_put(tree, 10, 10);
        bplus_tree_put(tree, 15, 15);
        bplus_tree_put(tree, 18, 18);
        bplus_tree_put(tree, 22, 22);
        bplus_tree_put(tree, 27, 27);
        bplus_tree_put(tree, 34, 34);
        bplus_tree_put(tree, 40, 40);
        bplus_tree_put(tree, 44, 44);
        bplus_tree_put(tree, 47, 47);
        bplus_tree_put(tree, 54, 54);
        bplus_tree_put(tree, 67, 67);
        bplus_tree_put(tree, 72, 72);
        bplus_tree_put(tree, 74, 74);
        bplus_tree_put(tree, 78, 78);
        bplus_tree_put(tree, 81, 81);
        bplus_tree_put(tree, 84, 84);
        bplus_tree_dump(tree);

        fprintf(stderr, "key:24 data:%d\n", bplus_tree_get(tree, 24));
        fprintf(stderr, "key:72 data:%d\n", bplus_tree_get(tree, 72));
        fprintf(stderr, "key:1 data:%d\n", bplus_tree_get(tree, 1));
        fprintf(stderr, "key:39 data:%d\n", bplus_tree_get(tree, 39));
        fprintf(stderr, "key:53 data:%d\n", bplus_tree_get(tree, 53));
        fprintf(stderr, "key:63 data:%d\n", bplus_tree_get(tree, 63));
        fprintf(stderr, "key:90 data:%d\n", bplus_tree_get(tree, 90));
        fprintf(stderr, "key:88 data:%d\n", bplus_tree_get(tree, 88));
        fprintf(stderr, "key:15 data:%d\n", bplus_tree_get(tree, 15));
        fprintf(stderr, "key:10 data:%d\n", bplus_tree_get(tree, 10));
        fprintf(stderr, "key:44 data:%d\n", bplus_tree_get(tree, 44));
        fprintf(stderr, "key:68 data:%d\n", bplus_tree_get(tree, 68));

        /* Not found */
        fprintf(stderr, "key:100 data:%d\n", bplus_tree_get(tree, 100));

        /* Clear all */
        fprintf(stderr, "\n> Clear all...\n");
        for (i = 1; i <= 100; i++) {
                bplus_tree_put(tree, i, 0);
        }
        bplus_tree_dump(tree);

        /* Not found */
        fprintf(stderr, "key:100 data:%d\n", bplus_tree_get(tree, 100));
}

static void bplus_tree_insert_delete_test(struct bplus_tree *tree)
{
        int i, max_key = 100;

        fprintf(stderr, "\n> B+tree insertion and deletion testing...\n");

        /* Ordered insertion and deletion */
        fprintf(stderr, "\n-- Insert 1 to %d, dump:\n", max_key);
        for (i = 1; i <= max_key; i++) {
                bplus_tree_put(tree, i, i);
        }
        bplus_tree_dump(tree);

        fprintf(stderr, "\n-- Delete 1 to %d, dump:\n", max_key);
        for (i = 1; i <= max_key; i++) {
                bplus_tree_put(tree, i, 0);
        }
        bplus_tree_dump(tree);

        /* Ordered insertion and reversed deletion */
        fprintf(stderr, "\n-- Insert 1 to %d, dump:\n", max_key);
        for (i = 1; i <= max_key; i++) {
                bplus_tree_put(tree, i, i);
        }
        bplus_tree_dump(tree);

        fprintf(stderr, "\n-- Delete %d to 1, dump:\n", max_key);
        while (--i > 0) {
                bplus_tree_put(tree, i, 0);
        }
        bplus_tree_dump(tree);

        /* Reversed insertion and ordered deletion */
        fprintf(stderr, "\n-- Insert %d to 1, dump:\n", max_key);
        for (i = max_key; i > 0; i--) {
                bplus_tree_put(tree, i, i);
        }
        bplus_tree_dump(tree);

        fprintf(stderr, "\n-- Delete 1 to %d, dump:\n", max_key);
        for (i = 1; i <= max_key; i++) {
                bplus_tree_put(tree, i, 0);
        }
        bplus_tree_dump(tree);

        /* Reversed insertion and reversed deletion */
        fprintf(stderr, "\n-- Insert %d to 1, dump:\n", max_key);
        for (i = max_key; i > 0; i--) {
                bplus_tree_put(tree, i, i);
        }
        bplus_tree_dump(tree);

        fprintf(stderr, "\n-- Delete %d to 1, dump:\n", max_key);
        for (i = max_key; i > 0; i--) {
                bplus_tree_put(tree, i, 0);
        }
        bplus_tree_dump(tree);
}

static void bplus_tree_normal_test(void)
{
        struct bplus_tree *tree;
        struct bplus_tree_config config;

        fprintf(stderr, "\n>>> B+tree normal test.\n");

        /* Init b+tree */
        config.order = 4;
        config.entries = 4;
        tree = bplus_tree_init(config.order, config.entries);
        if (tree == NULL) {
                fprintf(stderr, "Init failure!\n");
                exit(-1);
        }

        /* getter and setter test */
        bplus_tree_get_put_test(tree);

        /* insertion and deletion test */
        bplus_tree_insert_delete_test(tree);

        /* Deinit b+tree */
        bplus_tree_deinit(tree);
}
//*****************************************************************

//*****************************************************************
//BSBM indexing

//#define st_size 2500
#include <stdbool.h>


/*
uint32_t inline MurmurOAAT32 ( const char * key)
{
	uint32_t h(3323198485ul);
      	for (;*key;++key)
	{
		h ^= *key;
	    	h *= 0x5bd1e995;
	    	h ^= h >> 15;
      	}
      	return h;
}

uint64_t inline MurmurOAAT64 ( const char * key)
{
      	uint64_t h(525201411107845655ull);
      	for (;*key;++key)
	{
		h ^= *key;
	    	h *= 0x5bd1e9955bd1e995;
	    	h ^= h >> 47;
      	}
      	return h;
}
*/




/*
 *
 * The K/V store is implemented by Masoud (Pouya) Salehpour, based on: https://codereview.stackexchange.com/questions/63493/simple-key-value-store-in-c-take-2
 *Please visit the original webpage for further details
 *
 *
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>

#define KVS_SPACE_INCREMENT 8

typedef int KVScompare(const char *a, const char *b);

typedef const void KVSkey;

typedef void KVSvalue;

typedef struct {
    KVSkey *key;
    KVSvalue *value;
} KVSpair;

typedef struct KVSstore KVSstore;

/** Create a new key-value store.

    @param compare
        A function to compare keys. If the store will only contain string keys,
        use strcmp, or use NULL for the default behavior of comparing memory
        addresses, or use a custom function matching the signature of strcmp.

    @return
        A pointer to the store.
*/
KVSstore *kvs_create(KVScompare *compare);

/** Destroy a key-value store.

    @param store
        A pointer to the store.
*/
void kvs_destroy(KVSstore *store);

/** Store a value.

    @param store
        A pointer to the store.

    @param key
        A key used to retrieve the value later. If the key already exists, the
        new value will be stored in place of the old one, unless value is NULL
        in which case the key-value pair will be removed from the store.

    @param value
        A pointer to the data being stored, or NULL to remove an existing value. 
*/
void kvs_put(KVSstore *store, KVSkey *key, KVSvalue *value);

/** Retrieve a value.

    @param store
        A pointer to the store.

    @param key
        A key used to retrieve the value.

    @return
        A pointer to the retrieved value, or NULL if not found.
*/
KVSvalue *kvs_get(KVSstore *store, KVSkey *key);

/** Remove a value from the store.

    @param store
        A pointer to the store.

    @param key
        A key identifying the value to be removed.
*/
void kvs_remove(KVSstore *store, KVSkey *key);

/** Get the number of values in a store.

    @param store
        A pointer to the store.

    @return
        The number of values contained in the store.
*/
size_t kvs_length(KVSstore *store);

/** Get a key-value pair at a given index.

    @param store
        A pointer to the store.

    @param index
        The index of the key-value pair.

    @return
        A pointer to the key-value pair, or NULL if the index is out or range.
*/
KVSpair *kvs_pair(KVSstore *store, size_t index);

#ifdef __cplusplus
}
#endif












struct KVSstore {
    KVSpair *pairs;
    KVScompare *compare;
    size_t length;
    size_t space;
};

static const size_t kvs_pair_size = sizeof(KVSpair);

static const size_t kvs_store_size = sizeof(KVSstore);

static KVSpair *kvs_search(KVSstore *store, KVSkey *key, int exact) {
    KVSpair *pairs = store->pairs;
    size_t lbound = 0;
    size_t rbound = store->length;
    while (lbound < rbound) {
        size_t index = lbound + ((rbound - lbound) >> 1);
        KVSpair *element = pairs + index;
        int result = store->compare(key, element->key);
        if (result < 0) {
            rbound = index;
        } else if (result > 0) {
            lbound = index + 1;
        } else {
            return element;
        }
    }
    return exact ? NULL : pairs + lbound;
}

static KVSpair *kvs_get_pair(KVSstore *store, KVSkey *key) {
    if ((!store) || (!store->pairs)) {
        return NULL;
    }
    return kvs_search(store, key, 1);
}

static void kvs_abort_if_null(void *pointer, const char *message) {
    if (pointer == NULL) {
        fprintf(stderr, "%s\n", message);
        exit(-1);
    }
}

static void kvs_resize_pairs(KVSstore *store, size_t amount) {
    if (!store) {
        return;
    }
    store->length += amount;
    if (store->space > store->length) {
        return;
    }
    store->space += KVS_SPACE_INCREMENT;
    store->pairs = realloc(store->pairs, kvs_pair_size * store->space);
    kvs_abort_if_null(store->pairs, "out of memory");
}

static size_t kvs_get_pair_index(KVSstore *store, KVSpair *pair) {
    if ((!store) || (!pair)) {
        return -1;
    }
    return pair - store->pairs;
}

static size_t kvs_get_bytes_from_pair(KVSstore *store, KVSpair *pair) {
    size_t pair_index;
    if ((!store) || (!pair)) {
        return 0;
    }
    pair_index = kvs_get_pair_index(store, pair);
    return (store->length - pair_index) * kvs_pair_size;
}

static void kvs_create_pair(KVSstore *store, KVSkey *key, KVSvalue *value) {
    KVSpair *pair;
    if (!store) {
        return;
    }
    pair = kvs_search(store, key, 0);
    if (pair < store->pairs + store->length) {
        size_t bytes = kvs_get_bytes_from_pair(store, pair);
        memmove(pair + 1, pair, bytes);
    }
    pair->key = key;
    pair->value = value;
    kvs_resize_pairs(store, +1);
}

static void kvs_remove_pair(KVSstore *store, KVSpair *pair) {
    if ((!store) || (!pair)) {
        return;
    }
    memmove(pair, pair + 1, kvs_get_bytes_from_pair(store, pair + 1));
    kvs_resize_pairs(store, -1);
}

static int kvs_compare_pointers(const char *a, const char *b) {
    return a - b;
}

KVSstore *kvs_create(KVScompare compare) {
    KVSstore *store = malloc(kvs_store_size);
    kvs_abort_if_null(store, "out of memory");
    store->pairs = NULL;
    store->length = 0;
    store->space = 0;
    if (compare) {
        store->compare = compare;
    } else {
        store->compare = kvs_compare_pointers;
    }
    kvs_resize_pairs(store, 0);
    return store;
}

void kvs_destroy(KVSstore *store) {
    if (!store) {
        return;
    }
    if (store->pairs) {
        free(store->pairs);
    }
    free(store);
}

void kvs_put(KVSstore *store, KVSkey *key, void *value) {
    KVSpair *pair = kvs_get_pair(store, key);
    if (pair) {
        if (value) {
            pair->value = value;
        } else {
            kvs_remove_pair(store, pair);
        }
    } else {
        kvs_create_pair(store, key, value);
    }
}

KVSvalue *kvs_get(KVSstore *store, KVSkey *key) {
    KVSpair *pair = kvs_get_pair(store, key);
    return pair ? pair->value : NULL;
}

void kvs_remove(KVSstore *store, KVSkey *key) {
    kvs_put(store, key, NULL);
}

size_t kvs_length(KVSstore *store) {
    if (!store) {
        return 0;
    }
    return store->length;
}

KVSpair *kvs_pair(KVSstore *store, size_t index) {
    if ((!store) || (index < 0) || (index >= store->length)) {
        return NULL;
    }
    return store->pairs + index;
}


/*
 *
 *End of the code for the K/V store
 original webpage: https://codereview.stackexchange.com/questions/63493/simple-key-value-store-in-c-take-2
 *
 */

struct bplus_tree *tree_SPO;

struct bplus_tree *tree_SOP;

struct bplus_tree *tree_PSO;

struct bplus_tree *tree_OSP;



static void bplus_tree_OSP_init(void)
{

        struct bplus_tree_config config;

        fprintf(stderr, "\n>>> B+tree SPO.\n");

        /* Init b+tree */
        config.order = 64;
        config.entries = 64;
        tree_OSP = bplus_tree_init(config.order, config.entries);
        if (tree_OSP == NULL) {
                fprintf(stderr, "Init failure!\n");
                exit(-1);
        }

        /* getter and setter test */
        //bplus_tree_get_put_test(tree);

        /* insertion and deletion test */
        //bplus_tree_insert_delete_test(tree);

        /* Deinit b+tree */
        //bplus_tree_deinit(tree);
}



static void bplus_tree_PSO_init(void)
{

        struct bplus_tree_config config;

        fprintf(stderr, "\n>>> B+tree SPO.\n");

        /* Init b+tree */
        config.order = 64;
        config.entries = 64;
        tree_PSO = bplus_tree_init(config.order, config.entries);
        if (tree_PSO == NULL) {
                fprintf(stderr, "Init failure!\n");
                exit(-1);
        }

        /* getter and setter test */
        //bplus_tree_get_put_test(tree);

        /* insertion and deletion test */
        //bplus_tree_insert_delete_test(tree);

        /* Deinit b+tree */
        //bplus_tree_deinit(tree);
}


static void bplus_tree_SPO_init(void)
{
        
        struct bplus_tree_config config;

        fprintf(stderr, "\n>>> B+tree SPO.\n");

        /* Init b+tree */
        config.order = 64;
        config.entries = 64;
        tree_SPO = bplus_tree_init(config.order, config.entries);
        if (tree_SPO == NULL) {
                fprintf(stderr, "Init failure!\n");
                exit(-1);
        }

        /* getter and setter test */
        //bplus_tree_get_put_test(tree);

        /* insertion and deletion test */
        //bplus_tree_insert_delete_test(tree);

        /* Deinit b+tree */
        //bplus_tree_deinit(tree);
}



static void bplus_tree_SOP_init(void)
{

        struct bplus_tree_config config;

        fprintf(stderr, "\n>>> B+tree SPO.\n");

        /* Init b+tree */
        config.order = 64;
        config.entries = 64;
        tree_SOP = bplus_tree_init(config.order, config.entries);
        if (tree_SOP == NULL) {
                fprintf(stderr, "Init failure!\n");
                exit(-1);
        }

        /* getter and setter test */
        //bplus_tree_get_put_test(tree);

        /* insertion and deletion test */
        //bplus_tree_insert_delete_test(tree);

        /* Deinit b+tree */
        //bplus_tree_deinit(tree);
}


static void bplus_tree_put_OSP(struct bplus_tree *tree, long long key_OS, long value_P)
{


        bplus_tree_put(tree, key_OS, value_P);

}






static void bplus_tree_put_PSO(struct bplus_tree *tree, int key_PS, int value_O)
{


        bplus_tree_put(tree, key_PS, value_O);

}




static void bplus_tree_put_SPO(struct bplus_tree *tree, int key_SP, int value_O)
{


	bplus_tree_put(tree, key_SP, value_O);

}


static void bplus_tree_put_SOP(struct bplus_tree *tree, int key_SO, int value_P)
{


        bplus_tree_put(tree, key_SO, value_P);

}




long s_counter=1, p_counter=1, o_counter=1;
long line_number=0;


KVSstore *s_store_str_id;
KVSstore *s_store_id_str;

KVSstore *p_store_str_id;
KVSstore *p_store_id_str;

KVSstore *o_store_str_id;
KVSstore *o_store_id_str;



void triple_construct(char* _ss, char* _pp, char* _oo)
{

	if ( kvs_get(s_store_str_id, _ss) == NULL)
	{
		kvs_put(s_store_str_id, _ss, s_counter);

		char* sdup = strdup(_ss);

		int * ints = malloc(sizeof(int));
		ints = s_counter;
		kvs_put(s_store_id_str, ints ,sdup);
		
		s_counter++;
		
	}


        
        if (kvs_get(p_store_str_id, _pp) == NULL )
        {
                kvs_put(p_store_str_id, _pp, p_counter);

	        char* pdup = strdup(_pp);

                int * intp = malloc(sizeof(int));
                intp = p_counter;
                kvs_put(p_store_id_str, intp ,pdup);

                p_counter++;

        }


        
        if (kvs_get(o_store_str_id, _oo) == NULL)
        {
                kvs_put(o_store_str_id, _oo, o_counter);

     		char* odup = strdup(_oo);

                int * into = malloc(sizeof(int));
                into = o_counter;
                kvs_put(o_store_id_str, into ,odup);

                o_counter++;

        }

	int slk = kvs_get(s_store_str_id, _ss);
	int plk = kvs_get(p_store_str_id, _pp);
	int olk = kvs_get(o_store_str_id, _oo);

	int nDigits=0;

	
	//nDigits = floor(log10(abs(plk))) + 1;
	
	// SPO
/*	
	int SP = (slk * (pow(10,N_DIGITS_P))) + plk;
	printf("\nSP:%d  o:%d\n", SP, olk);
	bplus_tree_put_SPO(tree_SPO, SP, olk);
	
*/

/*

	//SOP
        nDigits = floor(log10(abs(olk))) + 1;
        int SO = (slk * (pow(10,nDigits))) + olk;
        printf("\nSO:%d  P:%d\n", SO, plk);
        bplus_tree_put_SOP(tree_SOP, SO, plk);
	//getchar();

*/      
	/*
        //PSO
        nDigits = floor(log10(abs(slk))) + 1;
        int PS = (plk * (pow(10,nDigits))) + slk;
        printf("\n PS:%d  O:%d\n", PS, olk);
        bplus_tree_put_PSO(tree_PSO, PS, olk);
        //getchar();
	*/

	 //OSP
        nDigits = floor(log10(abs(slk))) + 1;
        long long OS = (olk * (pow(10,nDigits))) + slk;
        printf("\n OS:%d  P:%d\n", OS, plk);
        bplus_tree_put_OSP(tree_OSP, OS, plk);
        //getchar();



}



















FILE *fp = NULL;

void main_kb (char *argv)
{
	  char subject[st_size], tr_predicate[st_size], object[st_size];
	  int readfile = 0;
	  //size_t line_number=0;
	  bool flag_bnode = false;
  	  char *check_ws;
  	  char gt = '>';
  	  long fpos;
  

	
	  
  	  fp = fopen(argv, "r");
  	  if (fp == NULL)
  	  {
  		  printf ("Error opening the file\n\n");
  		  exit(EXIT_FAILURE);
  	  } else
  	  {
		  while (readfile != EOF)
		  {
			  line_number++;

			  fpos = ftell(fp);
  			  readfile = fscanf(fp, "%s %s %[^\n]\n", subject, tr_predicate, object);
  
			  if (subject[0] == '_' && subject[1]==':')
  				  flag_bnode = true;
  			  else
  				  check_ws = strstr(subject,">");
  
			  if (!flag_bnode && check_ws == NULL)
  			  {
				fseek(fp, fpos, SEEK_SET);
				readfile = fscanf(fp, "%[^>]> %s %[^\n]\n", subject, tr_predicate, object);
				strncat(subject, &gt, 1);
			
			  }
  			  flag_bnode = false;

			  object[(strlen(object)-1)]='\0';

			  if (readfile == 3)
			  {
				    #if (DEBUG == 1)  
				  {
  				
					  printf("line:%zu \n s: %s \n  p:%s \n o:%s \n \n\n\n ", line_number, subject, tr_predicate, object);
					  printf("s_l: %d \t  p_l:%d \t o_l:%d \n ", strlen(subject), strlen(tr_predicate), strlen(object));

				
				  }
				    #endif

				  char* sdup = strdup(subject);
				  char* pdup = strdup(tr_predicate);
				  char* odup = strdup(object);

				  triple_construct(sdup , pdup, odup);


				  
				 
				  


			  }


		  }
	  
	  
	  }




}


int main(void)
{
	s_store_str_id = kvs_create(strcmp);
	p_store_str_id = kvs_create(strcmp);
	o_store_str_id = kvs_create(strcmp);

	s_store_id_str = kvs_create(NULL);
	p_store_id_str = kvs_create(NULL);
	o_store_id_str = kvs_create(NULL);


	//bplus_tree_SPO_init();
	
	//bplus_tree_SOP_init();
	//bplus_tree_PSO_init();
	  bplus_tree_OSP_init();

	//main_kb("/home/ubuntu/ds_compare/limitedBsbm.nt");
	//main_kb("/home/ubuntu/ds_compare/testds.nt");

	//bplus_tree_dump(tree_SPO);
	
	//main_kb("/home/ubuntu/ds_compare/bsbm.nt");
	  //main_kb("/home/ubuntu/ds_compare/bsbm.nt");
        //
          
	  

	//main_kb("/home/ubuntu/icwe2021datasets/bsbm/bsbm100k_sorted.nt");
        
	//main_kb("/home/ubuntu/icwe2021datasets/lubm/lubm100k_sorted.nt");
        //main_kb("/home/ubuntu/icwe2021datasets/sp2b/sp2b100k_sorted.nt");

        main_kb("/home/ubuntu/icwe2021datasets/bsbm/bsbm1m_sorted.nt");
        //main_kb("/home/ubuntu/icwe2021datasets/lubm/lubm1m_sorted.nt");
        //main_kb("/home/ubuntu/icwe2021datasets/sp2b/sp2b1m_sorted.nt");








	long long nDigits_o, SO, PS, nDigits_s, OS;

		
	//int nDigits = floor(log10(abs(p_counter))) + 1;
	//
	//
	//
	

	//nDigits_o = floor(log10(abs(o_counter-1))) + 1;

	//SO = ((s_counter-1) * (pow(10,nDigits_o))) + (o_counter-1);
	//
	printf("o_counter:%d      s_counter:%d \n ", o_counter-1, s_counter-1);

	nDigits_s = floor(log10(abs(s_counter-1))) + 1;

	printf("nDigits_s:%d \n", nDigits_s);
	OS = ((o_counter-1) * (pow(10,nDigits_s))) + (s_counter-1);

	//PS = ((p_counter-1) * (pow(10,nDigits_s))) + (s_counter-1);



	//int SP = ((s_counter-1) * (pow(10,N_DIGITS_P))) + (p_counter-1);
 
 
         //printf("\n last SP:%d \n", SP);
	 //printf("\n last SO:%d \n", SO);

	 printf("\n last OS:%d \n", OS);

	 //getchar();
	 

	 /****************************************
	  *  START SPO
	  *
	  ****************************************/


	 // to store execution time of code
     	 double time_spent = 0.0;
     	 clock_t begin = clock();

	time_t wc_begin = time(NULL);



	/*	
	for (long i=1; i<= SP ;i++)
	{
		bplus_tree_get(tree_SPO, i);
		//printf("\n*****************\n");
	}
	*/
	
	

	time_t wc_end = time(NULL);
	printf("WALL CLOCK -SPO- Time elpased is %d secondsi\n", (wc_end - wc_begin));


	clock_t end = clock();
    	// calculate elapsed time by finding difference (end - begin) and
	// dividing the difference by CLOCKS_PER_SEC to convert to seconds

    	time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
    	printf("CLOCK OS -SPO - Time elpased is %f seconds\n", time_spent);
         /****************************************
          *  END SPO
          *
          ****************************************/

	

 	  /****************************************
          *  START SOP
          *
          ****************************************/


	


         // to store execution time of code
         time_spent = 0.0;
         begin = clock();

         wc_begin = time(NULL);



	 /*
        
        for (long i=1; i<= SO ;i++)
        {
                bplus_tree_get(tree_SOP, i);
                //printf("\n*****************\n");
        }
	*/
        
        

        wc_end = time(NULL);
        printf("WALL CLOCK- SOP - Time elpased is %d secondsi\n", (wc_end - wc_begin));


        end = clock();
        // calculate elapsed time by finding difference (end - begin) and
        // dividing the difference by CLOCKS_PER_SEC to convert to seconds

        time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
        printf("CLOCK OS -SOP- Time elpased is %f seconds\n", time_spent);


	

          /****************************************
          *  END SOP
          *
          ****************************************/



          /****************************************
          *  START SPO
          *
          ****************************************/





         // to store execution time of code
         time_spent = 0.0;
         begin = clock();

         wc_begin = time(NULL);


	 /*
        for (long i=1; i<= PS ;i++)
        {
                bplus_tree_get(tree_PSO, i);
                //printf("\n*****************\n");
        }
	*/



        wc_end = time(NULL);
        printf("WALL CLOCK- SOP - Time elpased is %d secondsi\n", (wc_end - wc_begin));


        end = clock();
        // calculate elapsed time by finding difference (end - begin) and
        // dividing the difference by CLOCKS_PER_SEC to convert to seconds

        time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
        printf("CLOCK OS -SOP- Time elpased is %f seconds\n", time_spent);




          /****************************************
          *  END SPO
          *
          ****************************************/





          /****************************************
          *  START OSP
          *
          ****************************************/

         // to store execution time of code
         time_spent = 0.0;
         begin = clock();

         wc_begin = time(NULL);




        for (long long i=1; i<= OS ;i++)
        {
                bplus_tree_get(tree_OSP, i);
                //printf("\n*****************\n");
        }

        wc_end = time(NULL);
        printf("WALL CLOCK- OSP - Time elpased is %d secondsi\n", (wc_end - wc_begin));


        end = clock();
        // calculate elapsed time by finding difference (end - begin) and
        // dividing the difference by CLOCKS_PER_SEC to convert to seconds

        time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
        printf("CLOCK OS - OSP- Time elpased is %f seconds\n", time_spent);

          /****************************************
          *  END OSP
          *
          ****************************************/







	kvs_destroy(s_store_str_id);
	kvs_destroy(p_store_str_id);
	kvs_destroy(o_store_str_id);

	kvs_destroy(s_store_id_str);
	kvs_destroy(p_store_id_str);
	kvs_destroy(o_store_id_str);
	return 0;
}








































































