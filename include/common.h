#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

// Configuration Constants
#define MAX_BUFFER_SIZE 8192
#define MAX_FILENAME 256
#define MAX_USERNAME 64
#define MAX_PATH 512
#define MAX_COMMAND 1024
#define MAX_CLIENTS 100
#define MAX_STORAGE_SERVERS 50
#define MAX_FILES_PER_SS 1000
#define MAX_SENTENCE_LENGTH 1024
#define MAX_WORD_LENGTH 128
#define MAX_UNDO_HISTORY 10
#define MAX_CHECKPOINTS 20
#define MAX_TAG_LENGTH 64
#define MAX_FOLDER_DEPTH 10
#define MAX_ACCESS_REQUESTS 100

// Fault Tolerance Constants
#define HEARTBEAT_INTERVAL 2      // Heartbeat every 2 seconds
#define HEARTBEAT_TIMEOUT 6      // Mark SS as dead after 6 seconds
#define MAX_VERSION_NUMBER 999999

// Network Configuration
#define NAME_SERVER_IP "127.0.0.1"
#define NAME_SERVER_PORT 8080
#define INITIAL_SS_PORT 9000
#define INITIAL_CLIENT_PORT 10000

// Error Codes
#define SUCCESS 0
#define ERR_FILE_NOT_FOUND 1
#define ERR_PERMISSION_DENIED 2
#define ERR_FILE_EXISTS 3
#define ERR_INVALID_OPERATION 4
#define ERR_SERVER_UNAVAILABLE 5
#define ERR_NETWORK_ERROR 6
#define ERR_FILE_LOCKED 7
#define ERR_INVALID_SENTENCE 8
#define ERR_INVALID_WORD_INDEX 9
#define ERR_MEMORY_ERROR 10
#define ERR_FOLDER_NOT_FOUND 11
#define ERR_FOLDER_EXISTS 12
#define ERR_CHECKPOINT_NOT_FOUND 13
#define ERR_REQUEST_EXISTS 14
#define ERR_USER_NOT_FOUND 15
#define ERR_REPLICA_UNAVAILABLE 16
#define ERR_SYNC_FAILED 17
#define ERR_VERSION_CONFLICT 18
#define ERR_USER_ALREADY_LOGGED_IN 19
#define ERR_RESERVED_EXTENSION 20
#define ERR_UNKNOWN 99

// Operation Codes
#define OP_REGISTER_SS 100
#define OP_REGISTER_CLIENT 101
#define OP_CREATE 102
#define OP_DELETE 103
#define OP_READ 104
#define OP_WRITE 105
#define OP_INFO 106
#define OP_VIEW 107
#define OP_LIST_USERS 108
#define OP_ADD_ACCESS 109
#define OP_REM_ACCESS 110
#define OP_STREAM 111
#define OP_EXEC 112
#define OP_UNDO 113
#define OP_WRITE_LOCK 114
#define OP_WRITE_UNLOCK 115
#define OP_DISCONNECT 116
#define OP_UPDATE_METADATA 117
#define OP_ACQUIRE_LOCK 118
#define OP_RELEASE_LOCK 119
#define OP_CREATE_FOLDER 120
#define OP_MOVE_FILE 121
#define OP_VIEW_FOLDER 122
#define OP_CHECKPOINT 123
#define OP_VIEW_CHECKPOINT 124
#define OP_REVERT_CHECKPOINT 125
#define OP_LIST_CHECKPOINTS 126
#define OP_REQUEST_ACCESS 127
#define OP_LIST_REQUESTS 128
#define OP_APPROVE_REQUEST 129
#define OP_DENY_REQUEST 130
#define OP_HEARTBEAT 131
#define OP_SYNC_METADATA 132
#define OP_REPLICA_WRITE 133
#define OP_GET_FILE_VERSION 134
#define OP_SYNC_FILE 135
#define OP_REPLICA_CHECKPOINT 136
#define OP_ACK 200
#define OP_ERROR 400

// Access Permissions
#define ACCESS_NONE 0
#define ACCESS_READ 1
#define ACCESS_WRITE 2

// Word Node in Linked List
typedef struct WordNode {
    char word[MAX_WORD_LENGTH];
    struct WordNode* next;
} WordNode;

// Sentence Node in Linked List (contains linked list of words)
typedef struct SentenceNode {
    WordNode* words;
    int word_count;
    pthread_mutex_t lock;  // Pointer-based lock for concurrent write access
    struct SentenceNode* next;
} SentenceNode;

// File Structure (2D Linked List)
typedef struct FileStructure {
    SentenceNode* sentences;
    int sentence_count;
    int total_words;
    int total_chars;
} FileStructure;

// File Metadata
typedef struct FileMetadata {
    char filename[MAX_FILENAME];
    char owner[MAX_USERNAME];
    time_t created;
    time_t last_accessed;
    time_t last_modified;
    int size;
    int word_count;
    int char_count;
    int storage_server_id;
    int version;  // Version number for replication
} FileMetadata;

// File Version Entry (for .db metadata storage)
typedef struct FileVersion {
    char filename[MAX_FILENAME];
    int version;
    char owner[MAX_USERNAME];
    time_t last_modified;
    int size;
    struct FileVersion* next;
} FileVersion;

// Access Control Entry
typedef struct AccessEntry {
    char username[MAX_USERNAME];
    int permissions; // ACCESS_READ or ACCESS_WRITE
    struct AccessEntry* next;
} AccessEntry;

// Checkpoint Entry
typedef struct Checkpoint {
    char tag[MAX_TAG_LENGTH];
    char* content;  // Full file content at checkpoint
    time_t created;
    struct Checkpoint* next;
} Checkpoint;

// Access Request Entry
typedef struct AccessRequest {
    char filename[MAX_FILENAME];
    char requester[MAX_USERNAME];
    int requested_permission;  // ACCESS_READ or ACCESS_WRITE
    time_t requested_at;
    struct AccessRequest* next;
} AccessRequest;

// Folder Entry
typedef struct Folder {
    char folder_name[MAX_FILENAME];
    char path[MAX_PATH];  // Full path from root
    char owner[MAX_USERNAME];
    time_t created;
} Folder;

// Per-user folder path (allows each user to organize files independently)
typedef struct UserFolderPath {
    char username[MAX_USERNAME];
    char folder_path[MAX_PATH];
    struct UserFolderPath* next;
} UserFolderPath;

// Sentence locks removed - storage server handles locking with pointer-based mutexes

// File Info (combines metadata and access control)
typedef struct FileInfo {
    FileMetadata metadata;
    AccessEntry* acl; // Access Control List
    Checkpoint* checkpoints; // Linked list of checkpoints
    pthread_mutex_t lock;
    // Sentence locks removed - storage server handles locking with pointer-based mutexes
    UserFolderPath* user_folders; // Per-user folder paths (linked list)
} FileInfo;

// Storage Server Info
typedef struct StorageServerInfo {
    int id;
    char ip[INET_ADDRSTRLEN];
    int nm_port;
    int client_port;
    int is_active;
    FileMetadata files[MAX_FILES_PER_SS];
    int file_count;
    time_t last_heartbeat;
    int replica_id;      // ID of the replica storage server (-1 if none)
    int is_replica;      // 1 if this is a replica, 0 if actual server
    int actual_server_id; // ID of the actual server (if this is a replica)
} StorageServerInfo;

// Client Info
typedef struct ClientInfo {
    char username[MAX_USERNAME];
    char ip[INET_ADDRSTRLEN];
    int port;
    int socket_fd;
    time_t connected_at;
} ClientInfo;

// Request/Response Packet
typedef struct Packet {
    int operation;
    int error_code;
    char filename[MAX_FILENAME];
    char username[MAX_USERNAME];
    char data[MAX_BUFFER_SIZE];
    int data_len;
    int flags; // For additional flags like -a, -l, etc.
    int sentence_num; // Also used for char_count in OP_UPDATE_METADATA
    int word_index;   // Also used for word_count in OP_UPDATE_METADATA
    int ss_id;
    char ss_ip[INET_ADDRSTRLEN];
    int ss_port;
    char lock_token[64]; // For distributed locking
    char folder_path[MAX_PATH]; // For folder operations
    char checkpoint_tag[MAX_TAG_LENGTH]; // For checkpoint operations
} Packet;

// Logging
void log_message(const char* component, const char* level, const char* format, ...);
void log_request(const char* component, const char* client_ip, int port, const char* operation);
void log_response(const char* component, const char* client_ip, int port, int error_code);

// Network Utilities
int create_server_socket(int port);
int connect_to_server(const char* ip, int port);
int send_packet(int socket_fd, Packet* packet);
int receive_packet(int socket_fd, Packet* packet);

// Error Handling
const char* get_error_message(int error_code);

// Time Utilities
char* format_time(time_t t);

#endif // COMMON_H
