#ifndef NS_TRIE_H
#define NS_TRIE_H

#include "ns_state.h"

// Trie operations
TrieNode *create_trie_node();
void trie_insert(TrieNode *root, const char *filename, int file_index);
int trie_search(TrieNode *root, const char *filename);
void free_trie(TrieNode *node);

// Cache operations
void cache_put(const char *filename, int ss_id);
int cache_get(const char *filename);
void cache_invalidate(const char *filename);

#endif // NS_TRIE_H
