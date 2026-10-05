# Network File System - Architecture Diagrams

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     Network File System                         │
└─────────────────────────────────────────────────────────────────┘

                            Users
                              │
              ┌───────────────┼───────────────┐
              │               │               │
         ┌────▼────┐    ┌────▼────┐    ┌────▼────┐
         │ Client1 │    │ Client2 │    │ ClientN │
         │ (alice) │    │  (bob)  │    │ (carol) │
         └────┬────┘    └────┬────┘    └────┬────┘
              │               │               │
              └───────────────┼───────────────┘
                              │
                    ┌─────────▼──────────┐
                    │   Name Server      │
                    │   (Port 8080)      │
                    │                    │
                    │ • Trie Search      │
                    │ • Access Control   │
                    │ • Caching          │
                    │ • Load Balancing   │
                    └─────────┬──────────┘
                              │
              ┌───────────────┼───────────────┐
              │               │               │
        ┌─────▼─────┐   ┌────▼─────┐   ┌────▼─────┐
        │Storage    │   │Storage   │   │Storage   │
        │Server 1   │   │Server 2  │   │Server N  │
        │(Port 9001)│   │(Port 9002│   │(Port 900N│
        │           │   │          │   │          │
        │storage/ss1│   │storage/ss2  │storage/ssN│
        │ • file1   │   │ • file3  │   │ • fileN  │
        │ • file2   │   │ • file4  │   │          │
        └───────────┘   └──────────┘   └──────────┘
```

## Component Communication

### Registration Phase

```
Storage Server              Name Server              Client
     │                           │                     │
     │    1. REGISTER_SS         │                     │
     ├──────────────────────────>│                     │
     │    (ID, IP, Port, Files)  │                     │
     │                           │                     │
     │    2. ACK                 │                     │
     │<──────────────────────────┤                     │
     │                           │                     │
     │                           │   3. REGISTER_CLIENT│
     │                           │<────────────────────┤
     │                           │   (Username)        │
     │                           │                     │
     │                           │   4. ACK            │
     │                           ├────────────────────>│
     │                           │                     │
```

### Read Operation (Direct Connection)

```
Client                 Name Server              Storage Server
  │                         │                          │
  │   1. READ request       │                          │
  ├────────────────────────>│                          │
  │   (filename, username)  │                          │
  │                         │                          │
  │                         │  2. Check permissions    │
  │                         │     Search file          │
  │                         │     Get SS location      │
  │                         │                          │
  │   3. SS Info            │                          │
  │<────────────────────────┤                          │
  │   (IP, Port)            │                          │
  │                         │                          │
  │   4. Connect to SS      │                          │
  ├──────────────────────────────────────────────────>│
  │                         │                          │
  │   5. READ request       │                          │
  ├──────────────────────────────────────────────────>│
  │                         │                          │
  │   6. File content       │                          │
  │<──────────────────────────────────────────────────┤
  │                         │                          │
  │   7. Close connection   │                          │
  ├──────────────────────────────────────────────────>│
  │                         │                          │
```

### Create Operation (Forwarded by NM)

```
Client                 Name Server              Storage Server
  │                         │                          │
  │   1. CREATE request     │                          │
  ├────────────────────────>│                          │
  │                         │                          │
  │                         │  2. Select SS (round-robin)
  │                         │                          │
  │                         │  3. Forward CREATE       │
  │                         ├─────────────────────────>│
  │                         │                          │
  │                         │                          │  4. Create file
  │                         │                          │     on disk
  │                         │                          │
  │                         │  5. ACK                  │
  │                         │<─────────────────────────┤
  │                         │                          │
  │                         │  6. Update trie          │
  │                         │     Add to file list     │
  │                         │                          │
  │   7. ACK                │                          │
  │<────────────────────────┤                          │
  │                         │                          │
```

## File Structure (2D Linked List)

```
┌─────────────────────────────────────────────────────────────┐
│                    FileStructure                            │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ sentence_count: 3                                    │   │
│  │ total_words: 9                                       │   │
│  │ total_chars: 40                                      │   │
│  │ sentences: ──────┐                                   │   │
│  └─────────────────│┘                                   │   │
└────────────────────│─────────────────────────────────────┘
                     │
    ┌────────────────▼────────────────┐
    │    SentenceNode (Sentence 1)    │
    │  ┌──────────────────────────┐   │
    │  │ word_count: 3            │   │
    │  │ words: ──────┐           │   │
    │  │ next: ────────┼──────┐   │   │
    │  └───────────────│──────│───┘   │
    └──────────────────│──────│───────┘
                       │      │
        ┌──────────────▼─┐    │
        │   WordNode     │    │
        │ word: "Hello"  │    │
        │ next: ────┐    │    │
        └───────────│────┘    │
                    │         │
        ┌───────────▼──┐      │
        │   WordNode   │      │
        │ word: "world"│      │
        │ next: ────┐  │      │
        └───────────│──┘      │
                    │         │
        ┌───────────▼──┐      │
        │   WordNode   │      │
        │   word: "."  │      │
        │  next: NULL  │      │
        └──────────────┘      │
                              │
    ┌─────────────────────────▼───────┐
    │    SentenceNode (Sentence 2)    │
    │  ┌──────────────────────────┐   │
    │  │ word_count: 4            │   │
    │  │ words: ──────┐           │   │
    │  │ next: ────────┼──────┐   │   │
    │  └───────────────│──────│───┘   │
    └──────────────────│──────│───────┘
                       │      │
        (Similar structure)   │
                              │
    ┌─────────────────────────▼───────┐
    │    SentenceNode (Sentence 3)    │
    │  ┌──────────────────────────┐   │
    │  │ word_count: 2            │   │
    │  │ words: ──────┐           │   │
    │  │ next: NULL   │           │   │
    │  └───────────────│──────────┘   │
    └──────────────────│────────────────┘
                       │
        (Similar structure)
```

## Trie Data Structure

```
Text: "We have files: apple.txt, app.txt, banana.txt"

                         Root
                          │
            ┌─────────────┼─────────────┐
            │             │             │
            a             b             ...
            │             │
            p             a
            │             │
            p             n
            │             │
      ┌─────┴─────┐       a
      │           │       │
      .           l       n
      │           │       │
      t           e       a
      │           │       │
      x           .       .
      │           │       │
      t           t       t
      │           │       │
    [END]       [END]   [END]
    idx=1       idx=0   idx=2

Search "apple.txt": 
  Root → a → p → p → l → e → . → t → x → t → [END] → return idx 0
  Time: O(10) = O(m)

Search "app.txt":
  Root → a → p → p → . → t → x → t → [END] → return idx 1
  Time: O(8) = O(m)

Search "cherry.txt":
  Root → c → NULL → return -1 (not found)
  Time: O(1)
```

## Access Control List (ACL)

```
File: project.txt
Owner: alice

┌─────────────────────────────────────────────┐
│           FileInfo                          │
│  ┌──────────────────────────────────────┐   │
│  │ metadata:                            │   │
│  │   filename: "project.txt"            │   │
│  │   owner: "alice"                     │   │
│  │   storage_server_id: 1               │   │
│  │                                      │   │
│  │ acl: ──────┐                         │   │
│  └────────────│──────────────────────────   │
└───────────────│──────────────────────────────┘
                │
    ┌───────────▼──────────┐
    │   AccessEntry        │
    │  username: "bob"     │
    │  permissions: READ   │
    │  next: ──────┐       │
    └──────────────│───────┘
                   │
    ┌──────────────▼───────┐
    │   AccessEntry        │
    │  username: "carol"   │
    │  permissions: WRITE  │
    │  next: NULL          │
    └──────────────────────┘

Permissions Check:
• alice:  READ ✓  WRITE ✓  DELETE ✓  (owner)
• bob:    READ ✓  WRITE ✗  DELETE ✗
• carol:  READ ✓  WRITE ✓  DELETE ✗
• dave:   READ ✗  WRITE ✗  DELETE ✗  (no entry)
```

## Cache System

```
┌────────────────────────────────────────────────┐
│              Cache (LRU-style)                 │
│                                                │
│  Index 0: [file1.txt, ss_id=1, valid, t=100]  │
│  Index 1: [file2.txt, ss_id=1, valid, t=105]  │
│  Index 2: [file3.txt, ss_id=2, valid, t=110]  │
│  ...                                           │
│  Index 99: [fileN.txt, ss_id=1, valid, t=200] │
│                                                │
│  cache_index = 100 (circular)                  │
└────────────────────────────────────────────────┘

Lookup "file2.txt":
1. Iterate cache entries
2. Match filename
3. Check timestamp (t=105, now=150, diff=45 < 60) ✓
4. Return ss_id=1
   Time: O(100) = O(1)

Lookup "oldfile.txt":
1. Iterate cache entries  
2. Match filename
3. Check timestamp (t=50, now=150, diff=100 > 60) ✗
4. Return -1 (cache miss)
5. Query Trie → Update cache
```

## Write Operation Flow

```
Client                Storage Server              Disk
  │                         │                      │
  │   WRITE file 1          │                      │
  ├────────────────────────>│                      │
  │                         │                      │
  │                         │  1. Create write     │
  │                         │     session          │
  │                         │                      │
  │                         │  2. Load file        │
  │                         │     from disk        │
  │                         ├─────────────────────>│
  │                         │<─────────────────────┤
  │                         │                      │
  │                         │  3. Parse to 2D list │
  │                         │     (keep in memory) │
  │                         │                      │
  │                         │  4. Traverse to      │
  │                         │     sentence node    │
  │                         │                      │
  │                         │  5. Lock sentence    │
  │                         │     pointer directly │
  │                         │     pthread_mutex_lock(&sentence->lock)
  │                         │                      │
  │                         │  6. Save to undo     │
  │                         │     push_undo()      │
  │                         │                      │
  │   ACK (ready)           │                      │
  │<────────────────────────┤                      │
  │                         │                      │
  │   0 "Hello"             │                      │
  ├────────────────────────>│                      │
  │                         │  7. Update word 0    │
  │                         │     in locked node   │
  │   ACK                   │                      │
  │<────────────────────────┤                      │
  │                         │                      │
  │   1 "world"             │                      │
  ├────────────────────────>│                      │
  │                         │  8. Update word 1    │
  │                         │     in same node     │
  │   ACK                   │                      │
  │<────────────────────────┤                      │
  │                         │                      │
  │   ETIRW                 │                      │
  ├────────────────────────>│                      │
  │                         │  9. Unlock pointer   │
  │                         │     pthread_mutex_unlock(&sentence->lock)
  │                         │                      │
  │                         │  10. Convert to text │
  │                         │                      │
  │                         │  11. Save to disk    │
  │                         ├─────────────────────>│
  │                         │                      │
  │                         │  12. Close session   │
  │                         │      free memory     │
  │                         │                      │
  │   ACK (complete)        │                      │
  │<────────────────────────┤                      │
  │                         │                      │
```

## Stream Operation

```
Client                Storage Server
  │                         │
  │   STREAM request        │
  ├────────────────────────>│
  │                         │  Load file
  │                         │  Parse to 2D list
  │                         │
  │   "Hello"               │
  │<────────────────────────┤
  │                         │  sleep(0.1s)
  │                         │
  │   "world"               │
  │<────────────────────────┤
  │                         │  sleep(0.1s)
  │                         │
  │   "."                   │
  │<────────────────────────┤
  │                         │  sleep(0.1s)
  │                         │
  │   "How"                 │
  │<────────────────────────┤
  │                         │  sleep(0.1s)
  │                         │
  │   ...                   │
  │                         │
  │   "STOP"                │
  │<────────────────────────┤
  │                         │
```

## Undo Stack

```
File: myfile.txt

┌────────────────────────────────────────┐
│        Undo Stack (Stack-like)         │
│                                        │
│  Top ─→ Version 3: "Hello world!"     │
│         Version 2: "Hello world."      │
│         Version 1: "Hello."            │
│         Version 0: ""                  │
│  ────────────────────────────────────  │
│                                        │
│  MAX_UNDO_STACK = 10                   │
└────────────────────────────────────────┘

Operation: UNDO
1. Pop version 3 → Restore version 2
2. Save version 2 to disk
3. undo_top[file] = 1

Operation: WRITE (new change)
1. Clone current state (version 2)
2. Push clone to stack (becomes version 3)
3. Apply changes
4. Save to disk

Stack Full (10 versions):
1. Remove version 0 (oldest)
2. Shift all down
3. Add new version at top
```

## Thread Model

### Name Server

```
┌────────────────────────────────────────────────┐
│              Name Server Process               │
│                                                │
│  Main Thread                                   │
│    ├─ Accept connections                       │
│    ├─ Create handler thread per client/SS      │
│    └─ Wait for shutdown signal                 │
│                                                │
│  Client Handler Thread 1                       │
│    ├─ Receive request                          │
│    ├─ Acquire locks (file_lock, etc.)          │
│    ├─ Process operation                        │
│    ├─ Release locks                            │
│    └─ Send response                            │
│                                                │
│  Client Handler Thread 2                       │
│    └─ (same)                                   │
│                                                │
│  ...                                           │
│                                                │
│  SS Handler Thread 1                           │
│    └─ Persistent connection, process requests  │
│                                                │
└────────────────────────────────────────────────┘

Locks prevent race conditions:
• Two clients create same file → file_lock ensures one succeeds
• Two clients access same file → safe (read-only on NM side)
```

### Storage Server

```
┌────────────────────────────────────────────────┐
│            Storage Server Process              │
│                                                │
│  Main Thread                                   │
│    ├─ Connect to NM                            │
│    ├─ Handle NM requests (blocking loop)       │
│    └─ Clean shutdown                           │
│                                                │
│  Client Server Thread                          │
│    ├─ Accept client connections                │
│    └─ Create handler thread per client         │
│                                                │
│  Client Handler Thread 1                       │
│    ├─ Receive READ/WRITE/RELEASE_LOCK request  │
│    ├─ For WRITE:                               │
│    │   ├─ Find/create write session            │
│    │   ├─ Traverse to sentence node            │
│    │   ├─ Lock sentence pointer directly       │
│    │   └─ Update words in locked node          │
│    ├─ For RELEASE_LOCK (ETIRW):                │
│    │   ├─ Unlock sentence pointer              │
│    │   └─ Free write session                   │
│    └─ Send response                            │
│                                                │
│  Client Handler Thread 2                       │
│    └─ (same - can work on different sentence)  │
│                                                │
└────────────────────────────────────────────────┘

Concurrent writes to different sentences:
• Thread 1: WRITE file sentence 1 (locks sentence node #1 pointer)
• Thread 2: WRITE file sentence 2 (locks sentence node #2 pointer)  
• Both proceed independently with different locked pointers ✓
```

## Error Propagation

```
Client              Name Server         Storage Server
  │                      │                     │
  │  CREATE existing     │                     │
  ├─────────────────────>│                     │
  │                      │  Forward CREATE     │
  │                      ├────────────────────>│
  │                      │                     │
  │                      │                     │  Check file exists
  │                      │                     │  ✗ File exists
  │                      │                     │
  │                      │  ERR_FILE_EXISTS    │
  │                      │<────────────────────┤
  │                      │                     │
  │                      │  Log error          │
  │                      │                     │
  │  ERR_FILE_EXISTS     │                     │
  │<─────────────────────┤                     │
  │                      │                     │
  │  Display error       │                     │
  │  "File already exists"                     │
  │                      │                     │
```

---

## Packet Structure Visualization

```
┌──────────────────────────────────────────────────────┐
│                   Packet (Fixed Size)                │
├──────────────┬───────────────────────────────────────┤
│ Field        │ Size / Type                           │
├──────────────┼───────────────────────────────────────┤
│ operation    │ int (4 bytes)                         │
│ error_code   │ int (4 bytes)                         │
│ filename     │ char[256] (256 bytes)                 │
│ username     │ char[64] (64 bytes)                   │
│ data         │ char[8192] (8192 bytes)               │
│ data_len     │ int (4 bytes)                         │
│ flags        │ int (4 bytes)                         │
│ sentence_num │ int (4 bytes)                         │
│ word_index   │ int (4 bytes)                         │
│ ss_id        │ int (4 bytes)                         │
│ ss_ip        │ char[16] (16 bytes)                   │
│ ss_port      │ int (4 bytes)                         │
├──────────────┴───────────────────────────────────────┤
│ Total Size: ~8580 bytes                              │
└──────────────────────────────────────────────────────┘

Example: READ Request
┌──────────────┬───────────────────────────────────────┐
│ operation    │ 104 (OP_READ)                         │
│ filename     │ "myfile.txt\0\0\0..."                 │
│ username     │ "alice\0\0\0..."                      │
│ (other)      │ 0 / empty                             │
└──────────────┴───────────────────────────────────────┘

Example: READ Response
┌──────────────┬───────────────────────────────────────┐
│ operation    │ 200 (OP_ACK)                          │
│ error_code   │ 0 (SUCCESS)                           │
│ data         │ "Hello world. How are you?\0\0..."    │
│ data_len     │ 27                                    │
│ (other)      │ 0 / empty                             │
└──────────────┴───────────────────────────────────────┘
```

This visual guide should help understand the system architecture and data flows!
