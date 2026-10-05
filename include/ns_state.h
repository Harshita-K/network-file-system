#ifndef NS_STATE_H
#define NS_STATE_H

#include "common.h"

#define CACHE_SIZE 100
#define ALPHABET_SIZE 128

// Trie Node for efficient file search
typedef struct TrieNode
{
    struct TrieNode *children[ALPHABET_SIZE];
    int is_end_of_word;
    int file_index; // Index in files array
} TrieNode;

// Cache Entry for recent searches
typedef struct CacheEntry
{
    char filename[MAX_FILENAME];
    int ss_id;
    time_t timestamp;
    int valid;
} CacheEntry;

// Name Server State
typedef struct
{
    StorageServerInfo storage_servers[MAX_STORAGE_SERVERS];
    int ss_count;

    ClientInfo clients[MAX_CLIENTS];
    int client_count;

    FileInfo files[MAX_FILES_PER_SS * MAX_STORAGE_SERVERS];
    int file_count;

    TrieNode *file_trie; // For O(m) file search where m = filename length

    CacheEntry cache[CACHE_SIZE];
    int cache_index;
    
    Folder folders[MAX_FILES_PER_SS * MAX_STORAGE_SERVERS];
    int folder_count;
    
    AccessRequest *access_requests; // Linked list of access requests

    pthread_mutex_t ss_lock;
    pthread_mutex_t client_lock;
    pthread_mutex_t file_lock;
    pthread_mutex_t cache_lock;
    pthread_mutex_t folder_lock;
    pthread_mutex_t request_lock;

    int server_socket;
    int is_running;
} NameServer;

// Global name server state (defined in ns_main.c)
extern NameServer ns;

// Initialization
void init_name_server();

#endif // NS_STATE_H
