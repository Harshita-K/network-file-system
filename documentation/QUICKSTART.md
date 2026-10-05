# Quick Start Guide - Network File System

## Prerequisites
```bash
sudo apt-get update
sudo apt-get install build-essential libreadline-dev

# Optional: For AI Write feature
sudo apt-get install libcurl4-openssl-dev libcjson-dev
```

## Setup AI Write (Optional Novelty Feature)

To use the AI-powered content generation:

```bash
# Get API key from https://makersuite.google.com/app/apikey
export GEMINI_API_KEY="your-api-key-here"

# Test setup
./test_ai_setup.sh
```

## Build
```bash
make
```

## Run (Easy Method)
```bash
# Terminal 1 - Start all servers
make run-servers

# Terminal 2 - Start client
make run-client
```

## Run (Manual Method)

### Terminal 1 - Name Server
```bash
./name_server
```

### Terminal 2 - Storage Server 1
```bash
./storage_server 1 9001
```

### Terminal 3 - Storage Server 2
```bash
./storage_server 2 9002
```

### Terminal 4 - Client
```bash
./client
# Enter username when prompted
```

## Basic Commands

### File Operations
```bash
CREATE myfile.txt           # Create file
READ myfile.txt            # Read file
DELETE myfile.txt          # Delete file
INFO myfile.txt            # Get file info
VIEW                       # List your files
VIEW -al                   # List all files with details
```

### Writing to Files
```bash
WRITE myfile.txt 1         # Edit sentence 1 (word-by-word mode)
0 Hello                    # Update word 0
1 world                    # Update word 1
2 .                        # Add period
ETIRW                      # Finish editing

# AI Write Mode (Novelty Feature - requires GEMINI_API_KEY)
WRITE myfile.txt 1 -AI     # AI-powered content generation
> Write a sentence about network file systems
```

### Access Control
```bash
LIST                       # List all users
ADDACCESS -R myfile.txt bob    # Give bob read access
ADDACCESS -W myfile.txt alice  # Give alice write access
REMACCESS myfile.txt bob      # Remove bob's access
```

### Advanced
```bash
STREAM myfile.txt          # Stream file word-by-word
UNDO myfile.txt           # Undo last change
EXEC commands.sh          # Execute file as shell commands
```

## Stop Servers
```bash
make stop-servers
```

## Clean Up
```bash
make clean                 # Remove build files
make clean-all            # Remove everything (builds, logs, storage)
```

## Troubleshooting

### Port already in use
```bash
# Find and kill process using port 8080
sudo lsof -ti:8080 | xargs kill -9

# Or use make command
make stop-servers
```

### Can't find library
```bash
# Install readline
sudo apt-get install libreadline-dev

# Rebuild
make clean
make
```

### Permission denied on files
- Make sure you're the owner or have been granted access
- Use VIEW -al to see file ownership

### Connection refused
- Make sure Name Server is running first
- Wait 2 seconds after starting Name Server before starting Storage Servers
- Check logs in logs/ directory

## Example Session

```bash
# As user "alice"
nfs> CREATE project.txt
nfs> WRITE project.txt 1
> 0 This
> 1 is
> 2 my
> 3 project
> 4 .
> ETIRW
nfs> READ project.txt
nfs> ADDACCESS -W project.txt bob
nfs> LIST

# As user "bob" (in another client)
nfs> READ project.txt
nfs> WRITE project.txt 1
> 5 Great
> 6 work
> 7 !
> ETIRW
nfs> STREAM project.txt
```

## Architecture Summary

```
Client ←→ Name Server (Port 8080) ←→ Storage Server 1 (Port 9001)
                                   ←→ Storage Server 2 (Port 9002)
                                   ←→ Storage Server N (Port 900N)
```

- Name Server: Coordinates everything, manages permissions
- Storage Servers: Store actual files
- Clients: Connect directly to Storage Servers for data transfer

## File Structure

Files are stored as 2D linked lists:
- Main list: Sentences
- Each sentence: List of words
- Each sentence node has its own mutex for pointer-based locking
- Allows efficient concurrent editing of different sentences
- All writes to a sentence stay in same locked node until ETIRW

