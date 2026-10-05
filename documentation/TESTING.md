# Testing Guide - Network File System

## Prerequisites

```bash
# Make sure everything is built
make clean
make

# Create test files
make test-files
```

## Test Suite

### Test 1: Basic Setup and Connection

**Objective**: Verify all components can start and connect.

```bash
# Terminal 1
./name_server

# Expected output:
# Name Server started on port 8080

# Terminal 2
./storage_server 1 9001

# Expected in Terminal 1:
# [INFO] Storage Server 1 registered with X files

# Terminal 3
./client
# Enter username: testuser

# Expected in Terminal 1:
# [INFO] Client registered: testuser

# In Terminal 3:
nfs> LIST

# Expected output:
# Users:
# testuser
```

**Status**: ✓ Pass if all components connect without errors

---

### Test 2: File Creation

**Objective**: Create new files and verify persistence.

```bash
# In client terminal:
nfs> CREATE file1.txt
# Expected: File created successfully

nfs> CREATE file2.txt
# Expected: File created successfully

nfs> CREATE file1.txt
# Expected: ERROR: File already exists

nfs> VIEW
# Expected: 
# file1.txt
# file2.txt
```

**Verification**:
```bash
# In another terminal:
ls storage/ss1/
# Should show: file1.txt  file2.txt
```

**Status**: ✓ Pass if files are created and listed correctly

---

### Test 3: Read and Write

**Objective**: Write content and read it back.

```bash
nfs> WRITE file1.txt 1
> 0 Hello
> 1 world
> 2 .
> ETIRW
# Expected: Write operation completed

nfs> READ file1.txt
# Expected: Hello world.

nfs> WRITE file1.txt 1
> 3 How
> 4 are
> 5 you
> 6 ?
> ETIRW

nfs> READ file1.txt
# Expected: Hello world. How are you?
```

**Status**: ✓ Pass if content is written and read correctly

---

### Test 4: File Information

**Objective**: Verify metadata is tracked correctly.

```bash
nfs> INFO file1.txt
# Expected output should include:
# - File: file1.txt
# - Size: X bytes
# - Words: 6
# - Characters: 24 (approximately)
# - Sentences: 2
# - Last Modified: [timestamp]

nfs> VIEW -l
# Expected: Detailed listing with word count, char count, owner, timestamp
```

**Status**: ✓ Pass if metadata is accurate

---

### Test 5: Access Control

**Objective**: Test read/write permissions.

```bash
# Terminal 1 - Client 1 (alice)
./client
# Enter username: alice

nfs> CREATE shared.txt
nfs> WRITE shared.txt 1
> 0 Alice's
> 1 file
> 2 .
> ETIRW

nfs> LIST
# Expected: Shows alice and other users

nfs> ADDACCESS -R shared.txt bob
# Expected: Access granted successfully

nfs> ADDACCESS -W shared.txt charlie
# Expected: Access granted successfully

# Terminal 2 - Client 2 (bob)
./client
# Enter username: bob

nfs> VIEW
# Expected: Shows shared.txt

nfs> READ shared.txt
# Expected: Alice's file.

nfs> WRITE shared.txt 1
# Expected: ERROR: Permission denied (bob only has read access)

# Terminal 3 - Client 3 (charlie)
./client
# Enter username: charlie

nfs> READ shared.txt
# Expected: Alice's file.

nfs> WRITE shared.txt 1
> 3 Charlie
> 4 was
> 5 here
> 6 !
> ETIRW
# Expected: Write operation completed

# Back in Terminal 1 (alice)
nfs> READ shared.txt
# Expected: Alice's file. Charlie was here!

# Terminal 4 - Client 4 (dave)
./client
# Enter username: dave

nfs> READ shared.txt
# Expected: ERROR: Permission denied

nfs> VIEW
# Expected: shared.txt NOT listed (no access)

nfs> VIEW -a
# Expected: shared.txt IS listed (shows all files)
```

**Status**: ✓ Pass if permissions are enforced correctly

---

### Test 6: Concurrent Writing (Different Sentences)

**Objective**: Multiple users edit different sentences simultaneously.

```bash
# Setup (as alice):
nfs> CREATE concurrent.txt
nfs> WRITE concurrent.txt 1
> 0 Sentence
> 1 one
> 2 .
> ETIRW
nfs> WRITE concurrent.txt 2
> 0 Sentence
> 1 two
> 2 .
> ETIRW
nfs> ADDACCESS -W concurrent.txt bob

# Terminal 1 (alice) - Start editing sentence 1
nfs> WRITE concurrent.txt 1
> 0 Modified
# (DON'T type ETIRW yet - keep it open)

# Terminal 2 (bob) - Edit sentence 2 (should work)
nfs> WRITE concurrent.txt 2
> 0 Bob
> 1 edited
> 2 this
> 3 .
> ETIRW
# Expected: Write operation completed ✓

# Terminal 1 (alice) - Finish editing
> 1 by
> 2 Alice
> 3 .
> ETIRW

# Either terminal:
nfs> READ concurrent.txt
# Expected: Modified by Alice. Bob edited this.
```

**Status**: ✓ Pass if both writes succeed

---

### Test 7: Concurrent Writing (Same Sentence - Should Block)

**Objective**: Second user cannot edit same sentence until first finishes.

```bash
# Terminal 1 (alice)
nfs> WRITE concurrent.txt 1
> 0 Editing
# (Keep open, don't type ETIRW)

# Terminal 2 (bob) - Try to edit same sentence
nfs> WRITE concurrent.txt 1
# Expected: ERROR: File is locked for writing

# Terminal 1 (alice) - Finish
> ETIRW

# Terminal 2 (bob) - Try again
nfs> WRITE concurrent.txt 1
> 0 Now
> 1 works
> 2 .
> ETIRW
# Expected: Write operation completed ✓
```

**Status**: ✓ Pass if second write is blocked until first completes

---

### Test 8: Undo Functionality

**Objective**: Revert changes to previous versions.

```bash
nfs> CREATE undo_test.txt

nfs> WRITE undo_test.txt 1
> 0 Version
> 1 one
> 2 .
> ETIRW

nfs> READ undo_test.txt
# Expected: Version one.

nfs> WRITE undo_test.txt 1
> 3 Version
> 4 two
> 5 .
> ETIRW

nfs> READ undo_test.txt
# Expected: Version one. Version two.

nfs> UNDO undo_test.txt
# Expected: Undo successful

nfs> READ undo_test.txt
# Expected: Version one.

nfs> UNDO undo_test.txt
# Expected: Undo successful

nfs> READ undo_test.txt
# Expected: (empty file)
```

**Status**: ✓ Pass if undo restores previous versions

---

### Test 9: File Deletion

**Objective**: Only owner can delete files.

```bash
# As alice:
nfs> CREATE delete_test.txt
nfs> ADDACCESS -W delete_test.txt bob

# As bob:
nfs> DELETE delete_test.txt
# Expected: ERROR: Permission denied

# As alice:
nfs> DELETE delete_test.txt
# Expected: File deleted successfully

nfs> VIEW
# Expected: delete_test.txt NOT listed

# Verify on disk:
# ls storage/ss1/ should not show delete_test.txt
```

**Status**: ✓ Pass if only owner can delete

---

### Test 10: Streaming

**Objective**: Word-by-word file streaming.

```bash
nfs> CREATE stream_test.txt
nfs> WRITE stream_test.txt 1
> 0 This
> 1 is
> 2 a
> 3 streaming
> 4 test
> 5 .
> ETIRW

nfs> STREAM stream_test.txt
# Expected output (with 0.1s delay between words):
# Streaming: This is a streaming test .

# Time the output:
# Start: 0.0s
# "This": 0.1s
# "is": 0.2s
# "a": 0.3s
# "streaming": 0.4s
# "test": 0.5s
# ".": 0.6s
# Total: ~0.6 seconds
```

**Test mid-stream disconnect**:
```bash
# While streaming, in storage server terminal: Ctrl+C

# Expected in client:
# ERROR: Storage Server disconnected
```

**Status**: ✓ Pass if words stream with delays and error on disconnect

---

### Test 11: Command Execution

**Objective**: Execute file content as shell commands.

```bash
nfs> CREATE commands.sh
nfs> WRITE commands.sh 1
> 0 echo
> 1 "Hello
> 2 from
> 3 NFS"
> 4 ;
> 5 date
> ETIRW

nfs> EXEC commands.sh
# Expected output:
# Output:
# Hello from NFS
# [current date and time]

# Test with dangerous command (should execute, but limited by NM permissions):
nfs> CREATE dangerous.sh
nfs> WRITE dangerous.sh 1
> 0 ls
> 1 -la
> ETIRW

nfs> EXEC dangerous.sh
# Expected: Directory listing of Name Server's working directory
```

**Status**: ✓ Pass if commands execute on Name Server

---

### Test 12: Multiple Storage Servers

**Objective**: Files distributed across multiple servers.

```bash
# Terminal 1: Name Server
./name_server

# Terminal 2: Storage Server 1
./storage_server 1 9001

# Terminal 3: Storage Server 2
./storage_server 2 9002

# Terminal 4: Client
./client
# username: testuser

nfs> CREATE file_ss1.txt
# Should go to SS1 (round-robin)

nfs> CREATE file_ss2.txt
# Should go to SS2 (round-robin)

nfs> CREATE file_ss1_2.txt
# Should go to SS1 again

nfs> VIEW -l
# Expected: All 3 files listed

# Verify distribution:
# ls storage/ss1/ should show: file_ss1.txt, file_ss1_2.txt
# ls storage/ss2/ should show: file_ss2.txt
```

**Status**: ✓ Pass if files are distributed round-robin

---

### Test 13: Storage Server Reconnection

**Objective**: New SS can join after system is running.

```bash
# With Name Server and SS1 running:

# Terminal: New Storage Server
./storage_server 3 9003

# Expected in Name Server terminal:
# [INFO] Storage Server 3 registered with X files

# In client:
nfs> VIEW -l
# Expected: Files from all 3 storage servers listed

nfs> CREATE file_on_ss3.txt
# Should be created on one of the servers
```

**Status**: ✓ Pass if new SS registers and receives files

---

### Test 14: Efficient Search (Performance)

**Objective**: Verify O(m) search performance.

```bash
# Create many files:
nfs> CREATE a.txt
nfs> CREATE ab.txt
nfs> CREATE abc.txt
nfs> CREATE abcd.txt
# ... (create 100+ files with various names)

# Search should be fast regardless of file count
nfs> INFO abcd.txt
# Time: < 1ms (even with 1000 files)

# Cache test:
nfs> INFO abcd.txt
# Second access should be even faster (cache hit)
```

**Verification in code**:
```c
// Trie search is O(m) where m = filename length
// Not O(n) where n = number of files
```

**Status**: ✓ Pass if search is instant even with many files

---

### Test 15: Error Handling

**Objective**: Verify all error codes work correctly.

```bash
# ERR_FILE_NOT_FOUND
nfs> READ nonexistent.txt
# Expected: ERROR: File not found

# ERR_PERMISSION_DENIED
# (as bob, trying to delete alice's file)
nfs> DELETE alice_file.txt
# Expected: ERROR: Permission denied

# ERR_FILE_EXISTS
nfs> CREATE existing.txt
nfs> CREATE existing.txt
# Expected: ERROR: File already exists

# ERR_INVALID_SENTENCE
nfs> WRITE file.txt 999
# Expected: ERROR: Invalid sentence number

# ERR_FILE_LOCKED
# (two users editing same sentence)
# Expected: ERROR: File is locked for writing
```

**Status**: ✓ Pass if all errors are caught and reported

---

### Test 16: Sentence Delimiter Handling

**Objective**: Verify period, exclamation, question marks are sentence delimiters.

```bash
nfs> CREATE delimiters.txt
nfs> WRITE delimiters.txt 1
> 0 First
> 1 sentence
> 2 .
> 3 Second
> 4 !
> 5 Third
> 6 ?
> ETIRW

nfs> INFO delimiters.txt
# Expected: 
# Sentences: 3
# Words: 7 (including delimiters as words)

# Test "e.g." (should be 3 sentences)
nfs> CREATE example.txt
nfs> WRITE example.txt 1
> 0 e
> 1 .
> 2 g
> 3 .
> ETIRW

nfs> INFO example.txt
# Expected: Sentences: 2 (e. and g.)
```

**Status**: ✓ Pass if delimiters create sentence boundaries

---

### Test 17: Persistence

**Objective**: Data survives server restarts.

```bash
# Create and write file
nfs> CREATE persist.txt
nfs> WRITE persist.txt 1
> 0 This
> 1 should
> 2 persist
> 3 .
> ETIRW

# Exit client
nfs> EXIT

# Stop storage server (Ctrl+C in SS terminal)

# Restart storage server
./storage_server 1 9001

# Expected in NS terminal:
# [INFO] Storage Server 1 registered with X files
# (X should include persist.txt)

# Start new client
./client
# username: newuser

nfs> VIEW -a
# Expected: persist.txt is listed

nfs> READ persist.txt
# Expected: This should persist.
```

**Status**: ✓ Pass if files survive SS restart

---

### Test 18: Large File

**Objective**: Handle files larger than buffer size.

```bash
nfs> CREATE large.txt
nfs> WRITE large.txt 1
# Write 100+ words
> 0 word1
> 1 word2
> ... (continue for many words)
> 100 word100
> 101 .
> ETIRW

nfs> READ large.txt
# Expected: All words returned (may be truncated at buffer limit)

nfs> INFO large.txt
# Expected: Word count = 101, Size = appropriate
```

**Status**: ✓ Pass if large files work (within buffer limits)

---

### Test 19: Stress Test - Many Concurrent Clients

**Objective**: System handles multiple clients.

```bash
# Terminal 1-10: Start 10 clients with different usernames
./client
# username: user1, user2, ..., user10

# All clients simultaneously:
# Each client creates a file
user1> CREATE user1_file.txt
user2> CREATE user2_file.txt
...
user10> CREATE user10_file.txt

# Each client writes to their file
user1> WRITE user1_file.txt 1
> 0 Hello
> 1 from
> 2 user1
> 3 .
> ETIRW

# Verify no conflicts
user1> VIEW
# Expected: All 10 files listed
```

**Status**: ✓ Pass if all operations succeed without errors

---

### Test 20: Logging

**Objective**: Verify all operations are logged.

```bash
# Perform various operations
nfs> CREATE test.txt
nfs> READ test.txt
nfs> WRITE test.txt 1
...

# Check logs
cat logs/name_server.log
# Expected: 
# [timestamp] [REQUEST] From 127.0.0.1:port - Operation: Client Request
# [timestamp] [RESPONSE] To 127.0.0.1:port - Status: Success
# ...

cat logs/storage_server.log
# Expected: Similar format with storage operations logged
```

**Status**: ✓ Pass if all operations are logged with timestamps

---

## Automated Test Script

Create a file `test.sh`:

```bash
#!/bin/bash

echo "=== NFS Test Suite ==="

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Test counter
PASSED=0
FAILED=0

test_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ PASS${NC}: $2"
        ((PASSED++))
    else
        echo -e "${RED}✗ FAIL${NC}: $2"
        ((FAILED++))
    fi
}

# Build
echo "Building..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1
test_result $? "Build"

# Start servers
echo "Starting servers..."
./name_server > /dev/null 2>&1 &
NS_PID=$!
sleep 2

./storage_server 1 9001 > /dev/null 2>&1 &
SS1_PID=$!
sleep 1

# Test basic commands via expect script
# (Would need expect tool installed)

# Cleanup
kill $NS_PID $SS1_PID 2>/dev/null

echo ""
echo "=== Results ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi
```

Run with: `bash test.sh`

---

## Performance Benchmarks

### Search Performance

```bash
# Create 1000 files
for i in {1..1000}; do
    echo "CREATE file$i.txt" | ./client
done

# Time search
time echo "INFO file500.txt" | ./client
# Expected: < 10ms

# Time search with cache
time echo "INFO file500.txt" | ./client
time echo "INFO file500.txt" | ./client
# Second time should be faster (cache hit)
```

### Concurrent Operations

```bash
# 10 clients writing simultaneously
for i in {1..10}; do
    (echo -e "CREATE file$i.txt\nWRITE file$i.txt 1\n0 test\nETIRW\nEXIT" | ./client) &
done
wait

# Verify all 10 files created
echo "VIEW -a" | ./client
```

---

## Test Summary Checklist

- [ ] Test 1: Basic Setup and Connection
- [ ] Test 2: File Creation
- [ ] Test 3: Read and Write
- [ ] Test 4: File Information
- [ ] Test 5: Access Control
- [ ] Test 6: Concurrent Writing (Different Sentences)
- [ ] Test 7: Concurrent Writing (Same Sentence)
- [ ] Test 8: Undo Functionality
- [ ] Test 9: File Deletion
- [ ] Test 10: Streaming
- [ ] Test 11: Command Execution
- [ ] Test 12: Multiple Storage Servers
- [ ] Test 13: Storage Server Reconnection
- [ ] Test 14: Efficient Search
- [ ] Test 15: Error Handling
- [ ] Test 16: Sentence Delimiter Handling
- [ ] Test 17: Persistence
- [ ] Test 18: Large File
- [ ] Test 19: Stress Test
- [ ] Test 20: Logging

---

## Known Issues During Testing

1. **Readline history**: May not persist across sessions (expected)
2. **Buffer limits**: Files > 8KB may be truncated in single packet
3. **Port conflicts**: Ensure no other processes using ports 8080, 9001, etc.
4. **Zombie processes**: Use `make stop-servers` to clean up

---

## Debugging Failed Tests

```bash
# Enable verbose logging
# Edit source files, recompile with debug flags
make clean
CFLAGS="-g -O0 -DDEBUG" make

# Run with GDB
gdb ./name_server
(gdb) run
# (when crash occurs)
(gdb) backtrace

# Check system logs
dmesg | tail

# Monitor network
tcpdump -i lo port 8080

# Check process status
ps aux | grep name_server
netstat -tulpn | grep 8080
```

---

## Fault Tolerance Testing

### Test 20: Replication Pairing

**Objective**: Verify storage servers are correctly paired as actual and replica.

```bash
# Terminal 1
./name_server

# Terminal 2 - Start SS1 (actual server)
./storage_server 1 9001

# Expected in Terminal 1:
# [INFO] Storage Server 1 registered as ACTUAL server (replica: 2)

# Terminal 3 - Start SS2 (replica server)
./storage_server 2 9002

# Expected in Terminal 1:
# [INFO] Storage Server 2 registered as REPLICA server (actual: 1)

# Terminal 4 - Start SS3 (another actual server)
./storage_server 3 9003

# Expected in Terminal 1:
# [INFO] Storage Server 3 registered as ACTUAL server (replica: 4)
```

**Status**: ✓ Pass if pairing is correctly established

---

### Test 21: Heartbeat Monitoring

**Objective**: Verify heartbeat mechanism detects inactive servers.

```bash
# Terminal 1
./name_server

# Terminal 2
./storage_server 1 9001

# Wait 10 seconds, observe Terminal 1:
# [DEBUG] Heartbeat received from Storage Server 1
# (Should appear every 5 seconds)

# Terminal 2 - Kill storage server
Ctrl+C

# Wait 20 seconds, observe Terminal 1:
# [WARNING] Storage Server 1 marked as INACTIVE (timeout)

# Terminal 2 - Restart storage server
./storage_server 1 9001

# Observe Terminal 1:
# [INFO] Storage Server 1 registered...
# Heartbeats resume
```

**Status**: ✓ Pass if inactive detection works within 15 seconds

---

### Test 22: CREATE with Replication

**Objective**: Verify files are replicated to backup server.

```bash
# Terminal 1: Name Server
./name_server

# Terminal 2: SS1 (actual)
./storage_server 1 9001

# Terminal 3: SS2 (replica)
./storage_server 2 9002

# Terminal 4: Client
./client
Username: alice

nfs> CREATE test_replicated.txt

# Expected in Terminal 1:
# [INFO] File test_replicated.txt replicated to server 2

# Verify on both servers:
# Terminal 2:
ls storage/ss1/
# Should show: test_replicated.txt

# Terminal 3:
ls storage/ss2/
# Should show: test_replicated.txt
```

**Status**: ✓ Pass if file exists on both servers

---

### Test 23: Failover on READ

**Objective**: Verify reads failover to replica when primary is down.

```bash
# Setup: Create file and kill primary server
# Terminal 1: Name Server
./name_server

# Terminal 2: SS1 (actual)
./storage_server 1 9001

# Terminal 3: SS2 (replica)
./storage_server 2 9002

# Terminal 4: Client
./client
Username: bob

nfs> CREATE failover_test.txt
nfs> WRITE failover_test.txt 1 0 "Primary server data"
nfs> ETIRW failover_test.txt 1

# Terminal 2 - Kill primary server
Ctrl+C

# Wait 20 seconds for heartbeat to detect failure

# Terminal 4 - Try reading
nfs> READ failover_test.txt

# Expected in Terminal 1:
# [WARNING] Storage Server 1 is inactive, attempting failover to replica
# [INFO] Failover successful: using replica server 2 for file failover_test.txt

# Expected in Terminal 4:
# Primary server data

# File content should be readable from replica
```

**Status**: ✓ Pass if read succeeds from replica

---

### Test 24: Failover on WRITE

**Objective**: Verify writes failover to replica when primary is down.

```bash
# Setup similar to Test 23
# With primary server down and file existing on replica

nfs> WRITE failover_test.txt 1 3 " Additional data from replica"
nfs> ETIRW failover_test.txt 1
nfs> READ failover_test.txt

# Expected:
# Primary server data Additional data from replica

# Check metadata version incremented:
# Terminal 3:
cat storage/ss2/metadata.db
# Should show version increased
```

**Status**: ✓ Pass if write succeeds on replica

---

### Test 25: Round-Robin Skips Inactive Servers

**Objective**: Verify CREATE operations skip inactive servers.

```bash
# Terminal 1: Name Server
./name_server

# Terminal 2: SS1
./storage_server 1 9001

# Terminal 3: SS3
./storage_server 3 9003

# Terminal 4: SS2 (replica)
./storage_server 2 9002

# Terminal 5: Client
./client
Username: charlie

nfs> CREATE file1.txt
# Should go to SS1

nfs> CREATE file2.txt
# Should go to SS3

nfs> CREATE file3.txt
# Should go to SS1 (round-robin, skipping replica SS2)

# Terminal 2: Kill SS1
Ctrl+C

# Wait 20 seconds

nfs> CREATE file4.txt
# Should go to SS3 (only active actual server)

# Expected in Terminal 1:
# CREATE requests only go to SS1 and SS3, never to SS2 (replica)
# After SS1 dies, all CREATE go to SS3
```

**Status**: ✓ Pass if replicas are never selected for new files

---

### Test 26: Metadata Persistence

**Objective**: Verify metadata.db tracks file versions correctly.

```bash
# Terminal 1: Name Server
./name_server

# Terminal 2: SS1
./storage_server 1 9001

# Terminal 3: Client
./client
Username: dave

nfs> CREATE version_test.txt
nfs> WRITE version_test.txt 1 0 "Version 1"
nfs> ETIRW version_test.txt 1

# Check metadata:
cat storage/ss1/metadata.db
# Expected: version_test.txt|1|dave|<timestamp>|<size>

nfs> WRITE version_test.txt 1 2 " Version 2"
nfs> ETIRW version_test.txt 1

cat storage/ss1/metadata.db
# Expected: version_test.txt|2|dave|<timestamp>|<size>

# Version should increment with each write
```

**Status**: ✓ Pass if version increments correctly

---

### Test 27: Both Servers Down Error

**Objective**: Verify proper error when both primary and replica are unavailable.

```bash
# Terminal 1: Name Server
./name_server

# Terminal 2: SS1
./storage_server 1 9001

# Terminal 3: SS2
./storage_server 2 9002

# Terminal 4: Client
./client
Username: eve

nfs> CREATE both_down_test.txt

# Kill both servers
# Terminal 2: Ctrl+C
# Terminal 3: Ctrl+C

# Wait 20 seconds for heartbeat timeout

nfs> READ both_down_test.txt

# Expected in Terminal 1:
# [ERROR] Both primary and replica servers unavailable for file both_down_test.txt

# Expected in Terminal 4:
# Error: Server unavailable
```

**Status**: ✓ Pass if appropriate error is returned

---

### Test 28: Async Replication Non-Blocking

**Objective**: Verify CREATE operations don't wait for replication.

```bash
# Terminal 1: Name Server
./name_server

# Terminal 2: SS1
./storage_server 1 9001

# Do NOT start SS2 (replica unavailable)

# Terminal 3: Client
./client
Username: frank

# Measure time for CREATE
nfs> CREATE async_test.txt

# Expected: CREATE should complete quickly (< 1 second)
# Even though SS2 is unavailable

# Expected in Terminal 1:
# [WARNING] Replica server 2 not available for file async_test.txt

# File should still be created on SS1
ls storage/ss1/async_test.txt
# Should exist
```

**Status**: ✓ Pass if CREATE doesn't block on replica failure

---

### Test 29: Replication Logging

**Objective**: Verify replication events are logged correctly.

```bash
# Run Test 22 (CREATE with Replication)
# Then check logs:

cat logs/name_server.log | grep -i replica

# Expected entries:
# [INFO] Storage Server 1 registered as ACTUAL server (replica: 2)
# [INFO] Storage Server 2 registered as REPLICA server (actual: 1)
# [INFO] File test_replicated.txt replicated to server 2

cat logs/name_server.log | grep -i failover

# Should show failover events when servers go down
```

**Status**: ✓ Pass if replication is properly logged

---

### Test 30: Stress Test - Multiple Failures

**Objective**: Test system under multiple simultaneous failures.

```bash
# Start full setup: NS, SS1, SS2, SS3, SS4
# Create multiple files across servers

# Randomly kill servers:
# Kill SS1 (primary)
# Wait 5 seconds
# Kill SS3 (another primary)

# Attempt operations on files from dead servers
# Should failover to SS2 and SS4

# Restart SS1 and SS3
# Verify heartbeat restores them

# System should recover gracefully
```

**Status**: ✓ Pass if system handles cascading failures

---

## Running All Fault Tolerance Tests

```bash
# Automated test script (create as test_fault_tolerance.sh):
#!/bin/bash

echo "Running Fault Tolerance Test Suite..."

# Test 20-30 can be automated with expect scripts
# Or run manually following the steps above

# Example automation:
./name_server &
NS_PID=$!
sleep 2

./storage_server 1 9001 &
SS1_PID=$!
sleep 2

./storage_server 2 9002 &
SS2_PID=$!
sleep 2

# Test replication...
echo "Testing replication pairing..."
grep -q "ACTUAL server" logs/name_server.log && echo "✓ SS1 registered as actual"
grep -q "REPLICA server" logs/name_server.log && echo "✓ SS2 registered as replica"

# Cleanup
kill $SS2_PID $SS1_PID $NS_PID
wait
```

---

## Bonus Feature Tests

### Test 31: AI Write - Basic Functionality

**Objective**: Test AI-powered content generation.

**Prerequisites**:
```bash
# Install dependencies
sudo apt-get install libcurl4-openssl-dev libcjson-dev

# Set API key
export GEMINI_API_KEY="your-api-key-here"

# Verify setup
./test_ai_setup.sh
```

**Test**:
```bash
nfs> CREATE ai_test.txt

nfs> WRITE ai_test.txt 1 -AI
# Expected: Lock acquired for sentence 1 (token: ...)
# Expected: === AI Write Mode ===
# Expected: Enter your prompt for Gemini AI...

> Write a haiku about programming
# Expected: Calling Gemini API...
# Expected: === AI Response ===
# Expected: [AI-generated haiku]
# Expected: AI-generated content written successfully to sentence 1
# Expected: Lock released. AI write operation completed.

nfs> READ ai_test.txt
# Expected: [Display AI-generated content]
```

**Status**: ✓ Pass if AI response is written correctly

---

### Test 32: AI Write - Lock Mechanism

**Objective**: Verify AI write uses proper locking.

```bash
# Terminal 1 (alice):
nfs> WRITE ai_test.txt 1 -AI
> Write a paragraph about distributed systems
# (Don't press enter yet - leave prompt open)

# Terminal 2 (bob):
nfs> WRITE ai_test.txt 1
# Expected: ERROR: Sentence 1 is currently locked by another user

# Terminal 2:
nfs> WRITE ai_test.txt 2 -AI
> Write a sentence about consensus algorithms
# Expected: Should succeed (different sentence)
```

**Status**: ✓ Pass if locks prevent concurrent edits to same sentence

---

### Test 33: AI Write - Error Handling

**Objective**: Test error conditions and recovery.

**Test A - Missing API Key**:
```bash
unset GEMINI_API_KEY

nfs> WRITE test.txt 1 -AI
# Expected: ERROR: GEMINI_API_KEY environment variable not set.
# Expected: Please set it with: export GEMINI_API_KEY="..."
```

**Test B - Invalid File**:
```bash
export GEMINI_API_KEY="your-key"

nfs> WRITE nonexistent.txt 1 -AI
# Expected: ERROR: File not found
```

**Test C - Network Failure**:
```bash
# Disconnect from internet or use invalid API key
export GEMINI_API_KEY="invalid_key"

nfs> WRITE test.txt 1 -AI
> Test prompt
# Expected: ERROR: Failed to get response from Gemini API
# Expected: Lock should be released (verify by trying regular WRITE)
```

**Test D - Empty Prompt**:
```bash
nfs> WRITE test.txt 1 -AI
> 
# (Press enter with empty prompt)
# Expected: ERROR: Empty prompt
# Expected: Lock released
```

**Status**: ✓ Pass if all errors are handled gracefully

---

### Test 34: AI Write - Undo Integration

**Objective**: Verify AI writes can be undone.

```bash
nfs> CREATE undo_ai_test.txt

nfs> WRITE undo_ai_test.txt 1 -AI
> Write the word hello three times
# Expected: AI generates content

nfs> READ undo_ai_test.txt
# Note the AI-generated content

nfs> UNDO undo_ai_test.txt
# Expected: Successfully undone

nfs> READ undo_ai_test.txt
# Expected: File should be empty (back to original state)
```

**Status**: ✓ Pass if AI write is properly tracked in undo stack

---

### Test 35: AI Write - Concurrent AI Writes

**Objective**: Test multiple users using AI write simultaneously.

```bash
# Terminal 1 (alice):
nfs> WRITE concurrent.txt 1 -AI
> Write a fact about networks
# Wait for response

# Terminal 2 (bob):
nfs> WRITE concurrent.txt 2 -AI
> Write a fact about operating systems
# Wait for response

# Terminal 3 (charlie):
nfs> WRITE concurrent.txt 3 -AI
> Write a fact about databases
# Wait for response

nfs> READ concurrent.txt
# Expected: All three sentences should contain different AI-generated content
```

**Status**: ✓ Pass if all AI writes complete without conflicts

---

### Test 36: AI Write - Long Content Generation

**Objective**: Test AI write with complex prompts generating long responses.

```bash
nfs> CREATE long_ai.txt

nfs> WRITE long_ai.txt 1 -AI
> Write a detailed paragraph explaining how network file systems work, including client-server architecture, file operations, and concurrency control.

# Expected: AI generates lengthy response
# Expected: Content should be written successfully (within MAX_BUFFER_SIZE)

nfs> READ long_ai.txt
# Expected: Full AI response displayed
```

**Status**: ✓ Pass if long content is handled correctly

---

### Test 37: AI Write - Multiple Sentence Replacement

**Objective**: Test AI write followed by traditional write.

```bash
nfs> CREATE mixed.txt

nfs> WRITE mixed.txt 1 -AI
> Write a sentence about clouds

nfs> WRITE mixed.txt 1
> 0 Traditional
> 1 edit
> 2 .
> ETIRW
# Expected: Should overwrite AI-generated content

nfs> READ mixed.txt
# Expected: Traditional edit.
```

**Status**: ✓ Pass if AI and traditional writes can alternate

---

## Running All Tests

```bash
# Automated test script:
#!/bin/bash

echo "Running Complete Test Suite..."
echo ""

# Basic Tests (1-19)
echo "Running Basic Tests..."
# ... existing tests ...

# Fault Tolerance Tests (20-30)
echo "Running Fault Tolerance Tests..."
# ... existing tests ...

# Bonus Feature Tests (31-37)
if [ -n "$GEMINI_API_KEY" ]; then
    echo "Running AI Write Tests..."
    ./test_ai_setup.sh && echo "✓ AI Write setup verified"
    # Test 31-37 can be automated with expect scripts
else
    echo "⚠ Skipping AI Write tests (GEMINI_API_KEY not set)"
fi

echo ""
echo "Test suite complete!"
```

---

This comprehensive testing guide now covers all features including the AI Write novelty feature. Run through each test to ensure the system works correctly!


