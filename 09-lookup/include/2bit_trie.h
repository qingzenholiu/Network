#ifndef __TRIE_H__
#define __TRIE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

#define KEY 2
#define MAX_CHILD 4
#define  MASK(pre_len)  (u32) (~(~0 << (pre_len)) << (32 - (pre_len)))

typedef struct Tree{
	int match;
	u32 ip;
	int mask;
	int inode;
	struct Tree *child[MAX_CHILD];
}Node, *Trie_node;

Node* CreateTrie();
int extract(u32 ip, int offset);
void insert_node(Trie_node root, u32 ip, int mask, int inode);
void Leaf_Push(Trie_node *root, Node *former_match);
int search_node(Trie_node root, u32 ip);
int search_node_lp(Trie_node root, u32 ip);
void MBIT_Print_Tree(Trie_node tree);


#endif