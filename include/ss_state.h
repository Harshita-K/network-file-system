#ifndef SS_STATE_H
#define SS_STATE_H

#define _DEFAULT_SOURCE
#include "common.h"
#include "file_structure.h"

#define MAX_UNDO_STACK 10
#define MAX_ACTIVE_WRITES 100
#define MAX_OPEN_FILES 50

// Shared file cache for concurrent writes
typedef struct {
    char filename[MAX_FILENAME];
    FileStructure* file_structure;
    int active_sessions;  // Number of active write sessions on this file
    pthread_mutex_t cache_lock;
    int valid;
} SharedFileCache;

// Active write session tracking
typedef struct {
    char filename[MAX_FILENAME];
    char lock_token[64];
    SentenceNode* locked_sentence;     // Pointer to original sentence in shared cache
    SentenceNode* working_copy;        // User's private working copy (Copy-on-Write)
    int sentence_num;                  // Original sentence index when lock was acquired
    int active;
    int cache_index;  // Index into shared file cache
} ActiveWriteSession;

// Storage Server State
typedef struct
{
    int id;
    char base_path[MAX_PATH];
    int nm_socket;
    int client_server_socket;
    int is_running;
    pthread_rwlock_t file_locks[MAX_FILES_PER_SS]; // Changed to rwlock for concurrent reads
    
    // Shared file cache for concurrent writes
    SharedFileCache file_cache[MAX_OPEN_FILES];
    pthread_mutex_t cache_mgmt_lock;
    
    // Active write sessions
    ActiveWriteSession write_sessions[MAX_ACTIVE_WRITES];
    pthread_mutex_t sessions_lock;

    // Undo history: stores last N versions of each file
    FileStructure *undo_stack[MAX_FILES_PER_SS][MAX_UNDO_STACK];
    int undo_top[MAX_FILES_PER_SS];
    char undo_filenames[MAX_FILES_PER_SS][MAX_FILENAME];
    int undo_count;
} StorageServer;

// Global storage server state (defined in ss_main.c)
extern StorageServer ss;

// Initialization
void init_storage_server(int id);
void get_file_path(const char *filename, char *filepath);
void get_metadata_db_path(char *dbpath);

#endif // SS_STATE_H
