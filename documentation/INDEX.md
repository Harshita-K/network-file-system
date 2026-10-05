# Documentation Index

Welcome to the Network File System (NFS) documentation! This index will help you find the information you need.

## 📖 Documentation Files

### 1. [README.md](README.md) - **START HERE**
**Comprehensive project documentation**

Sections:
- Overview and architecture
- File structure (2D linked list)
- All features (150/150 points)
- Directory structure
- Compilation instructions
- Usage guide
- Commands reference
- Implementation details
- Assumptions and design decisions

**When to read**: First time setup, understanding system architecture

### 2. [QUICKSTART.md](QUICKSTART.md)
**Get up and running in 5 minutes**

Contents:
- Prerequisites
- Build instructions
- Quick start (automated)
- Manual start instructions
- Basic commands
- Example session
- Troubleshooting

**When to read**: Want to try the system quickly

### 3. [IMPLEMENTATION.md](IMPLEMENTATION.md)
**Deep dive into the code**

Sections:
- Code organization
- Key data structures
- Important algorithms (Trie, parsing, caching, undo)
- Thread safety
- Network protocol
- File format
- Performance characteristics
- Memory usage
- Debugging tips
- Common issues

**When to read**: Understanding how the code works, debugging, making changes

### 4. [DIAGRAMS.md](DIAGRAMS.md)
**Visual architecture and data flows**

Contents:
- System overview diagram
- Component communication flows
- File structure visualization
- Trie data structure
- Access control list
- Cache system
- Write operation flow
- Stream operation
- Undo stack
- Thread model
- Error propagation
- Packet structure

**When to read**: Visual learner, understanding system design

### 5. [TESTING.md](TESTING.md)
**Comprehensive testing guide**

Contents:
- 20 detailed test cases
- Test procedures
- Expected outputs
- Automated test script
- Performance benchmarks
- Test checklist
- Debugging failed tests

**When to read**: Verifying system works, debugging issues

### 6. [AI_WRITE_FEATURE.md](AI_WRITE_FEATURE.md)
**🌟 Novelty Feature - AI-Powered Content Generation**

Contents:
- Overview and command syntax
- Setup instructions (dependencies, API key)
- Usage examples and workflows
- How it works (API integration)
- Features and capabilities
- Error messages and troubleshooting
- Security considerations
- Implementation details
- Future enhancements

**When to read**: Want to use AI to generate file content, setting up Gemini API

### 7. [SUMMARY.md](SUMMARY.md)
**Project overview and quick reference**

Contents:
- What's implemented
- Feature checklist
- Technical highlights
- File organization
- Usage summary
- Assumptions
- Design decisions
- Code statistics
- Quick reference card

**When to read**: Quick overview, reference card

## 🚀 Quick Navigation

### I want to...

#### **Get started quickly**
→ [QUICKSTART.md](QUICKSTART.md) → Run `make run-servers` and `make run-client`

#### **Understand the architecture**
→ [README.md](README.md) - Architecture section
→ [DIAGRAMS.md](DIAGRAMS.md) - System overview

#### **Learn how files are stored**
→ [README.md](README.md) - File Structure Implementation
→ [DIAGRAMS.md](DIAGRAMS.md) - File structure visualization
→ [IMPLEMENTATION.md](IMPLEMENTATION.md) - Key data structures

#### **Understand the search algorithm**
→ [README.md](README.md) - Efficient Search section
→ [IMPLEMENTATION.md](IMPLEMENTATION.md) - Trie algorithm
→ [DIAGRAMS.md](DIAGRAMS.md) - Trie data structure

#### **Learn all commands**
→ [README.md](README.md) - Commands section
→ [SUMMARY.md](SUMMARY.md) - Quick reference card

#### **Compile and run**
→ [README.md](README.md) - Compilation and Usage sections
→ [QUICKSTART.md](QUICKSTART.md) - Build and run

#### **Test the system**
→ [TESTING.md](TESTING.md) - All 20 test cases

#### **Debug an issue**
→ [QUICKSTART.md](QUICKSTART.md) - Troubleshooting
→ [IMPLEMENTATION.md](IMPLEMENTATION.md) - Debugging tips
→ [TESTING.md](TESTING.md) - Debugging failed tests

#### **Understand the code**
→ [IMPLEMENTATION.md](IMPLEMENTATION.md) - Complete code walkthrough
→ Source files with inline comments

#### **See visual diagrams**
→ [DIAGRAMS.md](DIAGRAMS.md) - All ASCII diagrams

#### **Know what's implemented**
→ [SUMMARY.md](SUMMARY.md) - Feature checklist
→ [README.md](README.md) - Features section

## 📂 Source Code

### Header Files (`include/`)

**[common.h](include/common.h)**
- All constants and error codes
- Network configuration
- Core data structures (Packet, FileInfo, etc.)
- Function prototypes

**[file_structure.h](include/file_structure.h)**
- 2D linked list interface
- Word/sentence operations
- File parsing functions

### Implementation (`src/`)

**[src/common/common.c](src/common/common.c)**
- Network I/O (send/receive packets)
- Socket creation and connection
- Logging functions
- Error message mapping
- Time utilities

**[src/common/file_structure.c](src/common/file_structure.c)**
- 2D linked list implementation
- Text parsing to linked list
- File to text conversion
- Word/sentence operations
- Clone for undo

**[src/name_server/name_server.c](src/name_server/name_server.c)**
- Main Name Server logic
- Trie implementation
- Cache management
- Access control
- Request routing
- All operation handlers

**[src/storage_server/storage_server.c](src/storage_server/storage_server.c)**
- Storage Server implementation
- File persistence (disk I/O)
- Undo stack management
- Sentence locking
- Multi-threaded client handling

**[src/client/client.c](src/client/client.c)**
- Client interface
- Command parsing
- All 13 user commands
- Direct SS connections
- Readline integration

### Build System

**[Makefile](Makefile)**
- Compilation rules
- Run targets (run-servers, run-client, etc.)
- Clean targets
- Help information

## 📋 Cheat Sheets

### Build Commands
```bash
make              # Build all
make clean        # Clean build files
make clean-all    # Clean everything
make run-servers  # Start NS and 2 SS
make run-client   # Start client
make stop-servers # Stop all servers
make help         # Show help
```

### User Commands
```
CREATE <file>                    # Create file
READ <file>                      # Read file
WRITE <file> <sent#>             # Edit sentence
  <word#> <content>              # Update word
  ETIRW                          # Finish
DELETE <file>                    # Delete file
INFO <file>                      # File metadata
VIEW [-a] [-l]                   # List files
STREAM <file>                    # Stream content
UNDO <file>                      # Undo change
LIST                             # List users
ADDACCESS -R/-W <file> <user>    # Grant access
REMACCESS <file> <user>          # Remove access
EXEC <file>                      # Execute commands
EXIT                             # Quit
```

### Network Ports
```
Name Server:        127.0.0.1:8080
Storage Server 1:   127.0.0.1:9001
Storage Server 2:   127.0.0.1:9002
Storage Server N:   127.0.0.1:900N
```

### File Locations
```
Storage:   storage/ss1/, storage/ss2/, ...
Logs:      logs/name_server.log, logs/storage_server.log, logs/client.log
Build:     build/*.o
```

## 🎯 Learning Path

### Beginner Path
1. Read [SUMMARY.md](SUMMARY.md) - Get overview
2. Read [QUICKSTART.md](QUICKSTART.md) - Start system
3. Try basic commands (CREATE, READ, WRITE)
4. Read [README.md](README.md) - Commands section

### Intermediate Path
1. Complete Beginner Path
2. Read [README.md](README.md) - Architecture section
3. Read [DIAGRAMS.md](DIAGRAMS.md) - Visual understanding
4. Run [TESTING.md](TESTING.md) - Test cases 1-10
5. Explore source code with comments

### Advanced Path
1. Complete Intermediate Path
2. Read [IMPLEMENTATION.md](IMPLEMENTATION.md) - Full code walkthrough
3. Run [TESTING.md](TESTING.md) - All 20 tests
4. Modify code and experiment
5. Study algorithms (Trie, caching, undo stack)

## 🔍 Search Guide

### Find information about...

| Topic | File | Section |
|-------|------|---------|
| How to compile | README.md | Compilation |
| How to run | QUICKSTART.md | Run |
| All commands | README.md | Commands |
| File structure | README.md | File Structure Implementation |
| 2D linked list | IMPLEMENTATION.md | Key Data Structures |
| Trie search | IMPLEMENTATION.md | Important Algorithms |
| Access control | README.md | Features / Access Control |
| Concurrency | IMPLEMENTATION.md | Thread Safety |
| Network protocol | IMPLEMENTATION.md | Network Protocol |
| Error codes | common.h | Error Codes section |
| Testing | TESTING.md | All sections |
| Troubleshooting | QUICKSTART.md | Troubleshooting |

## 📊 File Statistics

```
Total Documentation: ~12,000 lines
  - README.md:         ~800 lines
  - QUICKSTART.md:     ~200 lines
  - IMPLEMENTATION.md: ~1,000 lines
  - DIAGRAMS.md:       ~800 lines
  - TESTING.md:        ~900 lines
  - SUMMARY.md:        ~400 lines

Total Code: ~2,500 lines
  - Name Server:       ~600 lines
  - Storage Server:    ~550 lines
  - Client:            ~550 lines
  - Common utilities:  ~800 lines

Total Files: 15+
  - Documentation:     7 files
  - Source:            5 files
  - Headers:           2 files
  - Build:             1 Makefile
```

## 🆘 Getting Help

### Quick Help
```bash
make help                    # Show Makefile targets
./client                     # Type HELP in client
man 2 socket                # System call documentation
```

### Documentation
1. Check this INDEX.md for navigation
2. README.md for comprehensive info
3. QUICKSTART.md for quick answers
4. IMPLEMENTATION.md for code details
5. Source code comments

### Debugging
1. Check logs in `logs/` directory
2. QUICKSTART.md - Troubleshooting section
3. IMPLEMENTATION.md - Debugging tips
4. TESTING.md - Debugging failed tests

## 📞 Contact

For course-related questions, contact the course staff at CS3-OSN-Monsoon-2025.

---

## Quick Links

- [README.md](README.md) - Complete documentation
- [QUICKSTART.md](QUICKSTART.md) - Quick start guide
- [IMPLEMENTATION.md](IMPLEMENTATION.md) - Code deep dive
- [DIAGRAMS.md](DIAGRAMS.md) - Visual diagrams
- [TESTING.md](TESTING.md) - Test procedures
- [SUMMARY.md](SUMMARY.md) - Project summary
- [Makefile](Makefile) - Build system

---

**Last Updated**: October 2025
**Project**: Network File System (NFS)
**Course**: CS3-OSN-Monsoon-2025
**Team**: Hot Single Threads Near You
