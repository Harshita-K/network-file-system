#include "../../include/ns_state.h"
#include "../../include/ns_trie.h"

// Trie functions
TrieNode *create_trie_node()
{
    TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
    if (!node)
        return NULL;

    node->is_end_of_word = 0;
    node->file_index = -1;

    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        node->children[i] = NULL;
    }

    return node;
}

void trie_insert(TrieNode *root, const char *filename, int file_index)
{
    TrieNode *curr = root;

    for (int i = 0; filename[i] != '\0'; i++)
    {
        int index = (unsigned char)filename[i];
        if (index >= ALPHABET_SIZE)
            continue;

        if (!curr->children[index])
        {
            curr->children[index] = create_trie_node();
        }
        curr = curr->children[index];
    }

    curr->is_end_of_word = 1;
    curr->file_index = file_index;
}

int trie_search(TrieNode *root, const char *filename)
{
    TrieNode *curr = root;

    for (int i = 0; filename[i] != '\0'; i++)
    {
        int index = (unsigned char)filename[i];
        if (index >= ALPHABET_SIZE || !curr->children[index])
            return -1;

        curr = curr->children[index];
    }

    if (curr && curr->is_end_of_word)
        return curr->file_index;

    return -1;
}

void free_trie(TrieNode *node)
{
    if (!node)
        return;

    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        free_trie(node->children[i]);
    }

    free(node);
}

// Cache functions
void cache_put(const char *filename, int ss_id)
{
    pthread_mutex_lock(&ns.cache_lock);

    ns.cache[ns.cache_index].valid = 1;
    strncpy(ns.cache[ns.cache_index].filename, filename, MAX_FILENAME - 1);
    ns.cache[ns.cache_index].ss_id = ss_id;
    ns.cache[ns.cache_index].timestamp = time(NULL);

    ns.cache_index = (ns.cache_index + 1) % CACHE_SIZE;

    pthread_mutex_unlock(&ns.cache_lock);
}

int cache_get(const char *filename)
{
    pthread_mutex_lock(&ns.cache_lock);

    for (int i = 0; i < CACHE_SIZE; i++)
    {
        if (ns.cache[i].valid && strcmp(ns.cache[i].filename, filename) == 0)
        {
            int ss_id = ns.cache[i].ss_id;
            pthread_mutex_unlock(&ns.cache_lock);
            return ss_id;
        }
    }

    pthread_mutex_unlock(&ns.cache_lock);
    return -1;
}

void cache_invalidate(const char *filename)
{
    pthread_mutex_lock(&ns.cache_lock);

    for (int i = 0; i < CACHE_SIZE; i++)
    {
        if (ns.cache[i].valid && strcmp(ns.cache[i].filename, filename) == 0)
        {
            ns.cache[i].valid = 0;
        }
    }

    pthread_mutex_unlock(&ns.cache_lock);
}
