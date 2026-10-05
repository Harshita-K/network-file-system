# Implementation Notes & Code Understanding

## Table of Contents
1. [Modular Architecture](#modular-architecture)
2. [Code Organization](#code-organization)
3. [Key Data Structures](#key-data-structures)
4. [Important Algorithms](#important-algorithms)
5. [Thread Safety](#thread-safety)
6. [Network Protocol](#network-protocol)
7. [File Format](#file-format)

---

## Modular Architecture

The codebase has been refactored into a modular architecture to improve maintainability, readability, and scalability. Each major component (Name Server, Storage Server, Client) is split into multiple files based on functional responsibilities.

### Design Principles

**Separation of Concerns**: Each module handles a specific aspect of functionality:
- **Main/Entry Point**: Initialization, signal handling, main loop
- **Network/Communication**: Client/server registration, connection management
- **Core Operations**: Business logic for file operations, command processing
- **Data Management**: Metadata, caching, state management
- **Utilities**: Helper functions, data structures

**Shared State Pattern**: Each component maintains a global state structure:
- `NameServer ns` (in ns_state.h): Centralized name server state
- `StorageServer ss` (in ss_state.h): Centralized storage server state
- `Client client` (in client_network.h): Client connection state

**Header-Implementation Separation**: Public interfaces defined in headers, implementation details in .c files

### Module Breakdown

#### Name Server (5 modules)
| Module | Responsibility | Key Functions |
|--------|---------------|---------------|
| ns_main.c | Entry point, initialization, signal handling | `main()`, `init_name_server()`, `heartbeat_monitor_thread()` |
| ns_trie.c | File search optimization | `trie_insert()`, `trie_search()`, `cache_get()`, `cache_put()` |
| ns_file_ops.c | File operation handlers | `handle_create()`, `handle_delete()`, `handle_view()`, `handle_info()`, `check_access()` |
| ns_storage.c | Storage server management | `register_storage_server()`, `async_replicate_create()`, `async_replicate_write()` |
| ns_client.c | Client handling | `register_client()`, `handle_client_request()` |

#### Storage Server (5 modules)
| Module | Responsibility | Key Functions |
|--------|---------------|---------------|
| ss_main.c | Entry point, initialization, client threads | `main()`, `init_storage_server()`, `handle_client()`, `register_with_nm()` |
| ss_metadata.c | Metadata persistence, versioning | `save_metadata_db()`, `load_metadata_db()`, `save_file_to_disk()`, `load_file_from_disk()` |
| ss_file_ops.c | Core file operations | `handle_create_file()`, `handle_read_file()`, `handle_write_file()`, `push_undo()`, `pop_undo()` |
| ss_cache.c | File caching, write sessions | `get_file_cache()`, `release_file_cache()`, `create_write_session()`, `close_write_session()` |
| ss_checkpoints.c | Checkpoint system | `handle_save_checkpoint()`, `handle_revert_checkpoint()`, `handle_list_checkpoints()` |

#### Client (4 modules)
| Module | Responsibility | Key Functions |
|--------|---------------|---------------|
| client_main.c | Entry point, main loop | `main()` |
| client_network.c | Network initialization | `init_client()`, `connect_to_nm()` |
| client_commands.c | Command processing | `cmd_create()`, `cmd_read()`, `cmd_write()`, `cmd_delete()`, `process_command()` |
| ai_write.c | AI integration | `call_gemini_api()`, `cmd_write_ai()` |

### Benefits of Modular Design

1. **Maintainability**: Easier to locate and fix bugs in specific functional areas
2. **Readability**: Each file has a clear, focused purpose (~200-400 lines vs 2000+ lines)
3. **Testability**: Individual modules can be tested in isolation
4. **Collaboration**: Multiple developers can work on different modules simultaneously
5. **Extensibility**: New features can be added by creating new modules or extending existing ones
6. **Build Efficiency**: Modified modules can be recompiled independently

### Compilation Flow

The Makefile compiles each module separately into object files, then links them:

```makefile
# Name Server
ns_main.o + ns_trie.o + ns_file_ops.o + ns_storage.o + ns_client.o + common.o → name_server

# Storage Server  
ss_main.o + ss_metadata.o + ss_file_ops.o + ss_cache.o + ss_checkpoints.o + common.o → storage_server

# Client
client_main.o + client_commands.o + client_network.o + ai_write.o + common.o → client
```

### Original Files

The original monolithic files (`name_server.c`, `storage_server.c`, `client.c`) are retained in the source tree for reference but are not compiled in the modular build. All functionality has been preserved and distributed across the new modular files.

---

## Code Organization

### Header Files (include/)

**common.h**
- Central header with all shared definitions
- Error codes, operation codes, network constants
- Core data structures (Packet, FileInfo, etc.)
- Function declarations for utilities

**file_structure.h**
- Interface for 2D linked list operations
- File parsing and manipulation functions
- Word and sentence operations

**ai_write.h**
- Interface for AI write functionality

**Name Server Headers:**
- `ns_state.h`: NameServer state structure definition
- `ns_trie.h`: Trie and cache function declarations
- `ns_file_ops.h`: File operation handlers
- `ns_storage.h`: Storage server management
- `ns_client.h`: Client registration and request handling

**Storage Server Headers:**
- `ss_state.h`: StorageServer state structure definition
- `ss_metadata.h`: Metadata database operations
- `ss_file_ops.h`: File operation handlers
- `ss_cache.h`: Cache and write session management
- `ss_checkpoints.h`: Checkpoint operations

**Client Headers:**
- `client_commands.h`: Command processing functions
- `client_network.h`: Network initialization

### Source Files (src/)

**src/common/**
- `common.c`: Network I/O, logging, time utilities
- `file_structure.c`: 2D linked list implementation

**src/name_server/** (Modular Architecture)
- `ns_main.c`: Entry point, initialization, signal handling, heartbeat monitoring
- `ns_trie.c`: Trie data structure for O(m) file search, LRU cache implementation
- `ns_file_ops.c`: File operations (create, delete, view, info, exec), access control, folder operations, access request system
- `ns_storage.c`: Storage server registration, replication (async threads), failover logic
- `ns_client.c`: Client registration and request handling thread
- `name_server.c`: Original monolithic file (kept for reference)

**src/storage_server/** (Modular Architecture)
- `ss_main.c`: Entry point, initialization, path utilities, client handler threads, registration with name server
- `ss_metadata.c`: Metadata database operations (save/load), file versioning, disk I/O (save/load file)
- `ss_file_ops.c`: Core file operations (create, delete, read, write with locking), undo operations, stream, info
- `ss_cache.c`: Shared file cache management, write session tracking, sentence locking helpers
- `ss_checkpoints.c`: Checkpoint operations (save, view, revert, list)
- `storage_server.c`: Original monolithic file (kept for reference)

**src/client/** (Modular Architecture)
- `client_main.c`: Entry point with username input loop and main command processor
- `client_commands.c`: All command handlers (VIEW, READ, WRITE, CREATE, DELETE, etc.) and command dispatcher
- `client_network.c`: Network initialization and Name Server connection
- `ai_write.c`: AI content generation (Novelty Feature) with Gemini API integration
- `client.c`: Original monolithic file (kept for reference)

---

## Key Data Structures

### 1. Packet (Network Communication)

```c
typedef struct Packet {
    int operation;           // OP_CREATE, OP_READ, etc.
    int error_code;         // SUCCESS, ERR_FILE_NOT_FOUND, etc.
    char filename[256];     // Target file
    char username[64];      // Requesting user
    char data[8192];        // Payload (file content, file list, etc.)
    int data_len;           // Length of data
    int flags;              // Operation flags (e.g., -a, -l for VIEW)
    int sentence_num;       // For WRITE operations
    int word_index;         // For WRITE operations
    int ss_id;              // Storage server ID
    char ss_ip[16];         // Storage server IP
    int ss_port;            // Storage server port
} Packet;
```

**Usage**: Sent over TCP sockets between all components. Fixed size for easy serialization.

### 2. FileStructure (2D Linked List)

```c
// Hierarchy: File → Sentences → Words
typedef struct FileStructure {
    SentenceNode* sentences;    // Head of sentence list
    int sentence_count;         // Total sentences
    int total_words;            // Total words across all sentences
    int total_chars;            // Total characters
} FileStructure;

typedef struct SentenceNode {
    WordNode* words;            // Head of word list
    int word_count;             // Words in this sentence
    struct SentenceNode* next;  // Next sentence
} SentenceNode;

typedef struct WordNode {
    char word[128];             // The actual word
    struct WordNode* next;      // Next word
} WordNode;
```

**Example in Memory**:
```
File: "Hello world. How are you?"

FileStructure
    └─ sentences ──→ SentenceNode (count=2, words=3)
                       └─ words ──→ WordNode("Hello")
                                    └─ next ──→ WordNode("world")
                                                └─ next ──→ WordNode(".")
                     └─ next ──→ SentenceNode (count=2, words=4)
                                   └─ words ──→ WordNode("How")
                                                └─ next ──→ WordNode("are")
                                                            └─ next ──→ WordNode("you")
                                                                        └─ next ──→ WordNode("?")
```

### 3. TrieNode (Efficient Search)

```c
typedef struct TrieNode {
    struct TrieNode* children[128];  // One pointer per ASCII char
    int is_end_of_word;              // 1 if valid filename ends here
    int file_index;                  // Index in files array
} TrieNode;
```

**Example Trie**:
```
Root
 ├─ 'f' → 'i' → 'l' → 'e' → '1' [end, index=0]
 │                       └─ '2' [end, index=1]
 └─ 't' → 'e' → 's' → 't' [end, index=2]
```

Search "file1": Root → 'f' → 'i' → 'l' → 'e' → '1' → return index 0
Time: O(5) = O(m) where m = len("file1")

### 4. AccessEntry (ACL)

```c
typedef struct AccessEntry {
    char username[64];              // User with access
    int permissions;                // ACCESS_READ (1) or ACCESS_WRITE (2)
    struct AccessEntry* next;       // Next ACL entry
} AccessEntry;
```

**Example ACL**:
```
File: project.txt (owner: alice)
ACL: bob (READ) → charlie (WRITE) → NULL

alice: Always has READ + WRITE (owner)
bob: Can READ, INFO, STREAM, EXEC
charlie: Can READ + WRITE, DELETE
david: No access (ERR_PERMISSION_DENIED)
```

---

## Important Algorithms

### 1. Trie Insert (Name Server)

```c
void trie_insert(TrieNode* root, const char* filename, int file_index) {
    TrieNode* curr = root;
    
    for (int i = 0; filename[i] != '\0'; i++) {
        int index = (unsigned char)filename[i];
        
        // Create node if doesn't exist
        if (!curr->children[index]) {
            curr->children[index] = create_trie_node();
        }
        
        curr = curr->children[index];
    }
    
    curr->is_end_of_word = 1;
    curr->file_index = file_index;
}
```

**Complexity**: O(m) where m = filename length
**Space**: O(m * 128 * sizeof(pointer)) per file

### 2. Text Parsing to 2D List (File Structure)

```c
FileStructure* parse_text_to_file(const char* text) {
    FileStructure* file = create_file_structure();
    SentenceNode* current_sentence = create_sentence_node();
    
    char word_buffer[128];
    int word_index = 0;
    
    for (int i = 0; text[i] != '\0'; i++) {
        if (is_space(text[i])) {
            // End of word
            if (word_index > 0) {
                word_buffer[word_index] = '\0';
                add_word_to_sentence(current_sentence, word_buffer);
                word_index = 0;
            }
        } 
        else if (is_sentence_delimiter(text[i])) {
            // End of word (if any)
            if (word_index > 0) {
                word_buffer[word_index] = '\0';
                add_word_to_sentence(current_sentence, word_buffer);
            }
            
            // Add delimiter as word
            char delimiter[2] = {text[i], '\0'};
            add_word_to_sentence(current_sentence, delimiter);
            
            // End sentence
            add_sentence_to_file(file, current_sentence);
            current_sentence = create_sentence_node();
            word_index = 0;
        }
        else {
            // Regular character
            word_buffer[word_index++] = text[i];
        }
    }
    
    // Add last sentence if non-empty
    if (current_sentence->word_count > 0) {
        add_sentence_to_file(file, current_sentence);
    }
    
    return file;
}
```

**Complexity**: O(n) where n = text length (single pass)

### 3. Cache Lookup (Name Server)

```c
int cache_get(const char* filename) {
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid && 
            strcmp(cache[i].filename, filename) == 0 &&
            time(NULL) - cache[i].timestamp < 60) {  // 60 sec TTL
            
            return cache[i].ss_id;  // Cache hit
        }
    }
    return -1;  // Cache miss
}
```

**Complexity**: O(CACHE_SIZE) = O(100) = O(1)
**Hit Rate**: High for frequently accessed files

### 4. Undo Stack (Storage Server)

```c
void push_undo(const char* filename, FileStructure* file) {
    int idx = get_file_index_for_undo(filename);
    FileStructure* clone = clone_file_structure(file);
    
    int top = undo_top[idx];
    
    if (top >= MAX_UNDO_STACK - 1) {
        // Stack full, remove oldest
        free_file_structure(undo_stack[idx][0]);
        
        // Shift all down
        for (int i = 0; i < MAX_UNDO_STACK - 1; i++) {
            undo_stack[idx][i] = undo_stack[idx][i + 1];
        }
        
        undo_stack[idx][MAX_UNDO_STACK - 1] = clone;
    } else {
        undo_stack[idx][++top] = clone;
        undo_top[idx] = top;
    }
}
```

**Stack Size**: 10 versions per file
**Memory**: Each version is a full file clone (~KB per version)

### 5. AI Content Generation (Novelty Feature)

```c
char* call_gemini_api(const char *prompt, const char *api_key) {
    CURL *curl;
    CURLcode res;
    
    // 1. Initialize curl for HTTPS request
    curl = curl_easy_init();
    
    // 2. Construct API URL with key
    char url[512];
    snprintf(url, sizeof(url), 
            "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%s",
            api_key);
    
    // 3. Build JSON request using cJSON
    cJSON *root = cJSON_CreateObject();
    cJSON *contents = cJSON_CreateArray();
    // ... construct request payload ...
    char *json_payload = cJSON_Print(root);
    
    // 4. Set HTTPS options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    
    // 5. Perform request
    res = curl_easy_perform(curl);
    
    // 6. Parse JSON response
    cJSON *response_json = cJSON_Parse(response_buffer);
    // Extract text from candidates[0].content.parts[0].text
    
    return response_text;
}
```

**Integration with Write Flow**:
```c
void cmd_write_ai(char *filename, char *sentence_num_str, ...) {
    // 1. Acquire lock (same as regular WRITE)
    Packet lock_request = {...};
    send_packet(nm_socket, &lock_request);
    
    // 2. Get user prompt
    char prompt[MAX_BUFFER_SIZE];
    fgets(prompt, sizeof(prompt), stdin);
    
    // 3. Call Gemini API
    char *ai_response = call_gemini_api(prompt, api_key);
    
    // 4. Write AI response as word index 0 (replaces sentence)
    Packet write_req = {
        .operation = OP_WRITE,
        .sentence_num = sentence_num,
        .word_index = 0,
        .data = ai_response,
        ...
    };
    send_packet(ss_socket, &write_req);
    
    // 5. Release lock
    Packet unlock_req = {...};
    send_packet(nm_socket, &unlock_req);
}
```

**Dependencies**:
- libcurl: HTTPS requests to Gemini API
- cjson: JSON parsing and serialization
- Environment: GEMINI_API_KEY for authentication

**Security**:
- API key read from environment (not hardcoded)
- TLS/SSL verification enabled
- Same locking mechanism prevents race conditions

---

## Thread Safety

### Name Server Locks

```c
pthread_mutex_t ss_lock;      // Protects storage_servers array
pthread_mutex_t client_lock;  // Protects clients array
pthread_mutex_t file_lock;    // Protects files array and trie
pthread_mutex_t cache_lock;   // Protects cache array
```

**Critical Sections**:
```c
// Adding new file
pthread_mutex_lock(&file_lock);
files[file_count++] = new_file;
trie_insert(file_trie, filename, file_count - 1);
pthread_mutex_unlock(&file_lock);

// Checking access
pthread_mutex_lock(&file_lock);
int has_access = check_access(filename, username, permission);
pthread_mutex_unlock(&file_lock);
```

### Storage Server Locks

```c
pthread_rwlock_t file_locks[MAX_FILES];     // Per-file metadata lock (read/write)
pthread_mutex_t sessions_lock;              // Protects active write sessions
```

**Pointer-Based Sentence Locking**:

The storage server implements pointer-based locking for concurrent sentence writes:

```c
// Each sentence node has its own lock
typedef struct SentenceNode {
    WordNode* words;
    int word_count;
    pthread_mutex_t lock;  // Pointer-based lock
    struct SentenceNode* next;
} SentenceNode;
```

**Write Session Flow**:
1. Client requests write lock from Name Server (coordinates via sentence number)
2. On first WRITE operation, Storage Server creates ActiveWriteSession:
   - Loads file from disk
   - Traverses to sentence N and locks that sentence node pointer
   - Keeps file structure in memory
3. Subsequent WRITEs in same session work on same locked sentence node (in memory)
4. On ETIRW:
   - Client sends OP_RELEASE_LOCK to Storage Server → closes session, unlocks pointer
   - Client sends OP_RELEASE_LOCK to Name Server → releases coordination lock

```c
// Before first WRITE (creates session)
session = create_write_session(filename, sentence_num, lock_token);
session->locked_sentence = get_sentence(file, sentence_num);
pthread_mutex_lock(&session->locked_sentence->lock);

// ... perform multiple writes to same locked sentence ...

// After WRITE (on ETIRW)
pthread_mutex_unlock(&session->locked_sentence->lock);
free_file_structure(session->file_in_memory);
session->active = 0;
```

**Benefits**: 
- Multiple users can edit different sentences of same file concurrently
- All writes to a sentence stay in the same locked node until ETIRW
- New sentence nodes only created after ETIRW releases the lock
- No index-based tracking needed - direct pointer locking

---

## Network Protocol

### Connection Types

1. **Name Server ↔ Storage Server**
   - Persistent connection
   - SS connects during registration
   - Used for CREATE, DELETE, forwarded operations

2. **Name Server ↔ Client**
   - Persistent connection per client
   - Client connects on startup
   - Used for all coordination

3. **Client ↔ Storage Server**
   - Temporary connections
   - Created for READ, WRITE, STREAM operations
   - Closed after operation completes

### Message Flow Examples

#### CREATE File

```
Client                  Name Server              Storage Server
  |                          |                          |
  |--OP_CREATE myfile.txt--->|                          |
  |                          |---OP_CREATE myfile.txt-->|
  |                          |                          |
  |                          |<---OP_ACK (SUCCESS)------|
  |<--OP_ACK (SUCCESS)-------|                          |
  |                          |                          |
```

#### READ File

```
Client                  Name Server              Storage Server
  |                          |                          |
  |--OP_READ myfile.txt----->|                          |
  |                          | (check access, get SS)   |
  |<--OP_ACK (SS info)-------|                          |
  |                                                      |
  |---(connect to SS)---------------------------------->|
  |--OP_READ myfile.txt------------------------------ ->|
  |<--OP_ACK (file content)-------------------------- --|
  |---(disconnect)------------------------------------->|
  |                          |                          |
```

#### WRITE File

```
Client                  Name Server              Storage Server
  |                          |                          |
  |--OP_WRITE file 1-------->|                          |
  |<--OP_ACK (SS info)-------|                          |
  |                                                      |
  |---(connect to SS)---------------------------------->|
  |--OP_WRITE file 1 word0 "Hello"--------------------->|
  |<--OP_ACK (SUCCESS)------------------------------------|
  |--OP_WRITE file 1 word1 "world"--------------------->|
  |<--OP_ACK (SUCCESS)------------------------------------|
  |--OP_WRITE_UNLOCK (ETIRW)--------------------------->|
  |<--OP_ACK (SUCCESS)------------------------------------|
  |---(disconnect)------------------------------------->|
  |                          |                          |
```

---

## File Format

### On Disk (Storage Server)

Files are stored as plain text in `storage/ssN/filename`:

```
# storage/ss1/myfile.txt
Hello world. How are you? I am fine!
```

### In Memory (2D Linked List)

Same file becomes:

```
FileStructure {
    sentence_count = 3
    total_words = 9
    total_chars = 40
    sentences = [
        Sentence 1 {
            word_count = 3
            words = ["Hello", "world", "."]
        },
        Sentence 2 {
            word_count = 4
            words = ["How", "are", "you", "?"]
        },
        Sentence 3 {
            word_count = 4
            words = ["I", "am", "fine", "!"]
        }
    ]
}
```

### Conversion Functions

```c
// Disk → Memory
FileStructure* load_file_from_disk(const char* filename);

// Memory → Disk
int save_file_to_disk(const char* filename, FileStructure* file);

// Memory → String
char* file_to_text(FileStructure* file);

// String → Memory
FileStructure* parse_text_to_file(const char* text);
```

---

## Error Handling Flow

### Example: Permission Denied

```c
// Client requests READ
Packet req;
req.operation = OP_READ;
strcpy(req.filename, "secret.txt");
strcpy(req.username, "bob");

// Name Server checks access
if (!check_access("secret.txt", "bob", ACCESS_READ)) {
    response.error_code = ERR_PERMISSION_DENIED;
    send_packet(client_socket, &response);
    log_response("name_server", client_ip, port, ERR_PERMISSION_DENIED);
    return;
}

// Client receives error
if (response.error_code != SUCCESS) {
    printf("ERROR: %s\n", get_error_message(response.error_code));
    // Output: "ERROR: Permission denied"
}
```

### Logging Example

```
# logs/name_server.log
[2024-10-20 15:30:46] [REQUEST] From 127.0.0.1:54321 - Operation: Client Request
[2024-10-20 15:30:46] [RESPONSE] To 127.0.0.1:54321 - Status: Permission denied

# logs/client.log
[2024-10-20 15:30:46] [ERROR] Operation failed: Permission denied
```

---

## Performance Characteristics

### File Search
- **Trie**: O(m) where m = filename length
- **Cache Hit**: O(1)
- **Cache Miss**: O(m)

### File Operations
- **Read**: O(n) where n = file size
- **Write Word**: O(s + w) where s = sentence number, w = word position
- **Undo**: O(n) (must clone entire file)
- **Stream**: O(n) with 0.1s delay per word

### Concurrency
- **Max Clients**: 100 (configurable)
- **Max Storage Servers**: 50 (configurable)
- **Max Concurrent Writes**: Limited by sentence count (different sentences can be written simultaneously)

---

## Memory Usage

### Name Server
- Fixed: ~10 MB (arrays, trie nodes)
- Per File: ~1 KB (metadata + ACL)
- Cache: ~100 KB (100 entries)

### Storage Server
- Fixed: ~5 MB (locks, buffers)
- Per File: Variable (depends on content)
- Undo Stack: ~10 × file size per file

### Client
- Fixed: ~1 MB (buffers, readline)
- Minimal dynamic allocation

---

## Debugging Tips

### Enable Verbose Logging

Edit source files to change log levels:
```c
// In common.c
void log_message(...) {
    // Always print DEBUG messages
    if (strcmp(level, "DEBUG") == 0) {
        printf(...);
    }
}
```

### Print Data Structures

```c
// In name_server.c
void print_trie(TrieNode* node, char* prefix, int depth) {
    if (node->is_end_of_word) {
        printf("File: %s (index=%d)\n", prefix, node->file_index);
    }
    for (int i = 0; i < 128; i++) {
        if (node->children[i]) {
            prefix[depth] = i;
            prefix[depth + 1] = '\0';
            print_trie(node->children[i], prefix, depth + 1);
        }
    }
}
```

### Monitor Sockets

```bash
# See all connections
netstat -tunap | grep name_server

# See open files
lsof -p $(pidof name_server)
```

### Check Memory Leaks

```bash
# Compile with debug symbols
make clean
CFLAGS="-g -O0" make

# Run with valgrind
valgrind --leak-check=full ./name_server
```

---

## Common Issues & Solutions

### Issue: "Address already in use"
**Cause**: Previous process didn't close socket
**Solution**: 
```bash
make stop-servers
# Or manually:
sudo lsof -ti:8080 | xargs kill -9
```

### Issue: "Connection refused"
**Cause**: Name Server not running
**Solution**: Start Name Server first, wait 2 seconds, then start others

### Issue: "File not found" but file exists
**Cause**: File not registered with Name Server
**Solution**: 
- If file created manually, restart Storage Server (will re-register)
- If created via CREATE command, check logs for errors

### Issue: Segmentation fault
**Cause**: Usually null pointer or buffer overflow
**Solution**: 
- Check all malloc() return values
- Validate packet data before use
- Use gdb: `gdb ./name_server`, then `run`, then `bt` on crash

### Issue: Concurrent write not working
**Cause**: Both trying to edit same sentence
**Solution**: Edit different sentences, or wait for ETIRW

---

This completes the implementation notes. For any questions, refer to:
1. README.md - High-level overview
2. QUICKSTART.md - Quick commands
3. This file - Deep dive into code
4. Source code comments - Line-by-line details

---

## BONUS FEATURES IMPLEMENTATION

### 1. Hierarchical Folder Structure (10 points)

**Data Structures:**
```c
typedef struct Folder {
    char folder_name[MAX_FILENAME];
    char path[MAX_PATH];  // Full path from root
    char owner[MAX_USERNAME];
    time_t created;
} Folder;

// Added to FileInfo:
typedef struct FileInfo {
    // ...existing fields...
    char folder_path[MAX_PATH];  // Location of this file
    // ...
} FileInfo;
```

**Name Server Extensions:**
- Array of `Folder` structures (up to MAX_FILES_PER_SS * MAX_STORAGE_SERVERS)
- New handlers: `handle_create_folder`, `handle_move_file`, `handle_view_folder`
- Folder path validation and hierarchy tracking
- Mutex protection for folder operations

**Operation Codes:**
- `OP_CREATE_FOLDER (120)`: Create new folder
- `OP_MOVE_FILE (121)`: Move file to different folder
- `OP_VIEW_FOLDER (122)`: List folder contents

**Implementation Notes:**
- Folders are metadata only (no disk representation)
- Files store their folder path for organization
- VIEWFOLDER shows both subfolders and files with ownership info
- Folder operations are permission-checked (only owner can move files)

### 2. Checkpoint Mechanism (15 points)

**Data Structures:**
```c
typedef struct Checkpoint {
    char tag[MAX_TAG_LENGTH];
    char* content;  // Full file content at checkpoint
    time_t created;
    struct Checkpoint* next;
} Checkpoint;

// Added to FileInfo:
typedef struct FileInfo {
    // ...existing fields...
    Checkpoint* checkpoints;  // Linked list of checkpoints
    // ...
} FileInfo;
```

**Storage Server Extensions:**
- Checkpoints saved as hidden files: `.checkpoint_<filename>_<tag>`
- New handlers: `handle_save_checkpoint`, `handle_view_checkpoint`, 
  `handle_revert_checkpoint`, `handle_list_checkpoints`
- Read locks for saving (non-blocking)
- Write locks for reverting (current version saved to undo stack first)

**Operation Codes:**
- `OP_CHECKPOINT (123)`: Save checkpoint with tag
- `OP_VIEW_CHECKPOINT (124)`: View checkpoint content
- `OP_REVERT_CHECKPOINT (125)`: Restore from checkpoint
- `OP_LIST_CHECKPOINTS (126)`: List available checkpoints

**Implementation Notes:**
- Checkpoints persist on disk as separate files
- Maximum 20 checkpoints per file (MAX_CHECKPOINTS)
- List uses shell command `ls -1` to enumerate checkpoint files
- Reverting saves current state to undo stack before restoring
- Thread-safe with rwlocks (concurrent reads of checkpoints allowed)

### 3. Access Request System (5 points)

**Data Structures:**
```c
typedef struct AccessRequest {
    char filename[MAX_FILENAME];
    char requester[MAX_USERNAME];
    int requested_permission;  // ACCESS_READ or ACCESS_WRITE
    time_t requested_at;
    struct AccessRequest* next;
} AccessRequest;
```

**Name Server Extensions:**
- Linked list of pending `AccessRequest` structures
- New handlers: `handle_request_access`, `handle_list_requests`,
  `handle_approve_request`, `handle_deny_request`
- Request validation (check if already exists)
- Owner-only approval (only file owner can approve/deny)

**Operation Codes:**
- `OP_REQUEST_ACCESS (127)`: Submit access request
- `OP_LIST_REQUESTS (128)`: View pending requests
- `OP_APPROVE_REQUEST (129)`: Grant access
- `OP_DENY_REQUEST (130)`: Reject access

**Implementation Notes:**
- Requests stored in memory (not persisted across restarts)
- LISTREQUESTS filters to show only owner's files
- APPROVE adds entry to file's ACL and removes request
- DENY simply removes request from list
- Maximum 100 pending requests (MAX_ACCESS_REQUESTS)
- Thread-safe with dedicated request_lock mutex

**Error Codes Added:**
- `ERR_FOLDER_NOT_FOUND (11)`: Folder doesn't exist
- `ERR_FOLDER_EXISTS (12)`: Folder already exists
- `ERR_CHECKPOINT_NOT_FOUND (13)`: Checkpoint tag not found
- `ERR_REQUEST_EXISTS (14)`: Access request already pending

**Packet Structure Extensions:**
```c
typedef struct Packet {
    // ...existing fields...
    char folder_path[MAX_PATH];        // For folder operations
    char checkpoint_tag[MAX_TAG_LENGTH]; // For checkpoint operations
} Packet;
```

---

## Fault Tolerance & Replication

### Replication Architecture

**Pairing Strategy:**
- Storage servers are paired: SS1 (actual) ↔ SS2 (replica), SS3 (actual) ↔ SS4 (replica)
- Odd-numbered servers (1, 3, 5...) are actual servers
- Even-numbered servers (2, 4, 6...) are replica servers
- Each actual server maintains a `replica_id` pointer to its replica
- Each replica server maintains an `actual_server_id` pointer to its primary

**Data Structures:**

```c
typedef struct StorageServerInfo {
    int id;
    int is_active;              // Set by heartbeat monitor
    int is_replica;             // 1 if replica, 0 if actual
    int replica_id;             // ID of replica server (-1 if none)
    int actual_server_id;       // ID of actual server (if this is replica)
    time_t last_heartbeat;      // Last successful heartbeat
    // ...other fields...
} StorageServerInfo;

typedef struct FileVersion {
    char filename[MAX_FILENAME];
    int version;                // Incremented on each write
    char owner[MAX_USERNAME];
    time_t last_modified;
    int size;
    struct FileVersion* next;
} FileVersion;
```

### Heartbeat Monitoring

**Thread: `heartbeat_monitor_thread()`**
- Runs every 5 seconds (HEARTBEAT_INTERVAL)
- Sends OP_HEARTBEAT to all registered storage servers
- Waits for OP_ACK response with 2-second timeout
- Marks server as inactive if:
  - No response within timeout
  - Connection fails
  - Server hasn't responded in 15 seconds (HEARTBEAT_TIMEOUT)

**Implementation:**
```c
while (ns.is_running) {
    sleep(HEARTBEAT_INTERVAL);
    for each storage server:
        if (time_since_last_heartbeat > HEARTBEAT_TIMEOUT):
            mark_as_inactive()
        else:
            send_heartbeat()
            if (response == OP_ACK):
                update_last_heartbeat()
            else:
                mark_as_inactive()
}
```

### Asynchronous Replication

**CREATE Operations:**
1. Name server selects active actual server (round-robin)
2. Forwards CREATE to actual server
3. On success, spawns async thread `async_replicate_create()`
4. Thread sends CREATE to replica server
5. No waiting for replica acknowledgment
6. Logs warning if replication fails

**WRITE Operations:**
1. Client connects to storage server
2. Storage server updates file in memory
3. On ETIRW (End The In Range Write), file saved to disk
4. Version number incremented in metadata.db
5. Future enhancement: async replication of updates

### Failover Logic

**READ/WRITE Request Routing:**
```c
if (primary_server.is_active):
    return primary_server_info
else:
    replica_id = primary_server.replica_id
    if (replica_server[replica_id].is_active):
        log_failover()
        return replica_server_info
    else:
        return ERR_SERVER_UNAVAILABLE
```

**Round-Robin Selection (CREATE):**
- Only considers active servers
- Skips replica servers (is_replica == 1)
- Cycles through actual servers only

### Metadata Persistence

**File: `storage/ssX/metadata.db`**
- Format: `filename|version|owner|last_modified|size`
- Updated after every successful write operation
- Loaded on SS startup for synchronization

**Functions:**
- `save_metadata_db()`: Persists file version info
- `load_metadata_db()`: Reads metadata on startup
- `get_file_version()`: Retrieves current version
- `increment_file_version()`: Increments version after write

### Recovery & Synchronization

**When SS Reconnects:**
1. Load metadata.db from disk
2. Send OP_SYNC_METADATA to name server with all file versions
3. Name server compares with replica's versions
4. Files with newer versions copied from replica to recovering SS
5. SS marked as active after sync complete

**Error Handling:**
- ERR_REPLICA_UNAVAILABLE: Both primary and replica down
- ERR_SYNC_FAILED: Synchronization process failed
- ERR_VERSION_CONFLICT: Version mismatch detected

### Thread Safety

**Replication Threads:**
- Async replication runs in detached threads
- No blocking on main request path
- Separate locks for replica_id access

**Heartbeat Thread:**
- Separate thread monitors all SS
- Updates is_active flag under ss_lock
- Read operations check is_active before routing

---

## Code Statistics (Modular Architecture)

### Overview

```
Language: C
Total Lines (Modular): ~8,400
Total Lines (Original): ~5,800
Components: 3 (Name Server, Storage Server, Client)
Headers: 15 (2 common + 13 module-specific)
Source Files: 17 modular files + 5 original files
Build System: Makefile with modular compilation
Documentation: 6 files (README, QUICKSTART, IMPLEMENTATION, DIAGRAMS, TESTING, INDEX)
```

### Line Count by Module

#### Name Server (2,344 lines across 5 modules)
| Module | Lines | Responsibility |
|--------|-------|----------------|
| ns_file_ops.c | 1,133 | File operations, access control, folder operations |
| ns_client.c | 575 | Client registration and request handling |
| ns_storage.c | 381 | Storage server management, replication |
| ns_main.c | 134 | Initialization, heartbeat monitoring |
| ns_trie.c | 121 | Trie search, cache management |

#### Storage Server (2,038 lines across 5 modules)
| Module | Lines | Responsibility |
|--------|-------|----------------|
| ss_file_ops.c | 642 | Core file operations, undo stack |
| ss_cache.c | 437 | File cache, write sessions |
| ss_main.c | 436 | Initialization, client handling |
| ss_metadata.c | 284 | Metadata persistence, versioning |
| ss_checkpoints.c | 239 | Checkpoint system |

#### Client (1,630 lines across 4 modules)
| Module | Lines | Responsibility |
|--------|-------|----------------|
| client_commands.c | 1,203 | All command handlers and dispatcher |
| ai_write.c | 301 | AI integration with Gemini API |
| client_main.c | 78 | Entry point and main loop |
| client_network.c | 48 | Network initialization |

#### Common (2,388 lines)
| Module | Lines | Responsibility |
|--------|-------|----------------|
| common.c | ~1,800 | Network utilities, logging |
| file_structure.c | ~588 | 2D linked list implementation |

### Comparison: Monolithic vs Modular

| Metric | Monolithic | Modular | Improvement |
|--------|-----------|---------|-------------|
| Name Server | 2,387 lines (1 file) | 2,344 lines (5 files) | Better organization |
| Storage Server | 2,063 lines (1 file) | 2,038 lines (5 files) | Better organization |
| Client | 1,331 lines (1 file) | 1,630 lines (4 files) | Better organization |
| Average File Size | 1,927 lines | 469 lines | 75% reduction |
| Max File Size | 2,387 lines | 1,203 lines | 50% reduction |
| Build Time (clean) | ~5 seconds | ~6 seconds | Marginal increase |
| Rebuild Time (1 file) | ~5 seconds | ~1 second | 80% faster |

### Header Files

| Category | Count | Files |
|----------|-------|-------|
| Common | 2 | common.h, file_structure.h |
| Name Server | 5 | ns_state.h, ns_trie.h, ns_file_ops.h, ns_storage.h, ns_client.h |
| Storage Server | 5 | ss_state.h, ss_metadata.h, ss_file_ops.h, ss_cache.h, ss_checkpoints.h |
| Client | 3 | client_commands.h, client_network.h, ai_write.h |

### Compilation Units

```
Total Object Files: 17
- common.o, file_structure.o (shared)
- ns_main.o, ns_trie.o, ns_file_ops.o, ns_storage.o, ns_client.o
- ss_main.o, ss_metadata.o, ss_file_ops.o, ss_cache.o, ss_checkpoints.o
- client_main.o, client_commands.o, client_network.o, ai_write.o

Executables: 3
- name_server (links 7 objects)
- storage_server (links 7 objects)
- client (links 6 objects)
```

### Code Complexity

| Component | Functions | Thread Entry Points | Mutexes/RWLocks |
|-----------|-----------|---------------------|-----------------|
| Name Server | 45+ | 2 (client handler, heartbeat) | 6 mutexes |
| Storage Server | 35+ | 2 (client handler, heartbeat) | file_locks[] + 3 mutexes |
| Client | 30+ | 1 (main thread) | 0 (single-threaded) |

### Benefits Achieved

1. **Modularity**: Each file has single, clear responsibility
2. **Maintainability**: Bug fixes localized to specific modules
3. **Readability**: Average file size reduced by 75%
4. **Build Efficiency**: Incremental compilation ~80% faster
5. **Testability**: Individual modules can be tested in isolation
6. **Scalability**: New features added by extending/adding modules
7. **Team Development**: Multiple developers can work on different modules

---


