# Network File System Makefile

CC = gcc
CFLAGS = -Wall -Wextra -pthread -I./include -g -Wno-unused-parameter -Wno-unused-variable -Wno-format-truncation -Wno-unused-but-set-variable
LDFLAGS = -pthread -lreadline
CLIENT_LDFLAGS = -pthread -lreadline -lcurl -lcjson

# Directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include
COMMON_DIR = $(SRC_DIR)/common
NS_DIR = $(SRC_DIR)/name_server
SS_DIR = $(SRC_DIR)/storage_server
CLIENT_DIR = $(SRC_DIR)/client

# Source files
COMMON_SRC = $(COMMON_DIR)/common.c $(COMMON_DIR)/file_structure.c

# Name Server modular sources
NS_SRC = $(NS_DIR)/ns_main.c $(NS_DIR)/ns_trie.c $(NS_DIR)/ns_file_ops.c \
         $(NS_DIR)/ns_storage.c $(NS_DIR)/ns_client.c

# Storage Server modular sources
SS_SRC = $(SS_DIR)/ss_main.c $(SS_DIR)/ss_metadata.c $(SS_DIR)/ss_file_ops.c \
         $(SS_DIR)/ss_cache.c $(SS_DIR)/ss_checkpoints.c

# Client modular sources
CLIENT_SRC = $(CLIENT_DIR)/client_main.c $(CLIENT_DIR)/client_commands.c $(CLIENT_DIR)/client_network.c
AI_WRITE_SRC = $(CLIENT_DIR)/ai_write.c

# Object files
COMMON_OBJ = $(BUILD_DIR)/common.o $(BUILD_DIR)/file_structure.o

# Name Server object files
NS_OBJ = $(BUILD_DIR)/ns_main.o $(BUILD_DIR)/ns_trie.o $(BUILD_DIR)/ns_file_ops.o \
         $(BUILD_DIR)/ns_storage.o $(BUILD_DIR)/ns_client.o

# Storage Server object files
SS_OBJ = $(BUILD_DIR)/ss_main.o $(BUILD_DIR)/ss_metadata.o $(BUILD_DIR)/ss_file_ops.o \
         $(BUILD_DIR)/ss_cache.o $(BUILD_DIR)/ss_checkpoints.o

# Client object files
CLIENT_OBJ = $(BUILD_DIR)/client_main.o $(BUILD_DIR)/client_commands.o $(BUILD_DIR)/client_network.o
AI_WRITE_OBJ = $(BUILD_DIR)/ai_write.o

# Executables
NAME_SERVER = name_server
STORAGE_SERVER = storage_server
CLIENT = client

# Default target
all: stop-servers clean-all build-all

build-all: directories $(NAME_SERVER) $(STORAGE_SERVER) $(CLIENT)

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p logs
	@mkdir -p storage

# Build common objects
$(BUILD_DIR)/common.o: $(COMMON_DIR)/common.c $(INCLUDE_DIR)/common.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/file_structure.o: $(COMMON_DIR)/file_structure.c $(INCLUDE_DIR)/file_structure.h $(INCLUDE_DIR)/common.h
	$(CC) $(CFLAGS) -c $< -o $@

# Build Name Server modules
$(BUILD_DIR)/ns_main.o: $(NS_DIR)/ns_main.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ns_trie.o: $(NS_DIR)/ns_trie.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ns_file_ops.o: $(NS_DIR)/ns_file_ops.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ns_storage.o: $(NS_DIR)/ns_storage.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ns_client.o: $(NS_DIR)/ns_client.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(NAME_SERVER): $(COMMON_OBJ) $(NS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Name Server built successfully"

# Build Storage Server modules
$(BUILD_DIR)/ss_main.o: $(SS_DIR)/ss_main.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ss_metadata.o: $(SS_DIR)/ss_metadata.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ss_file_ops.o: $(SS_DIR)/ss_file_ops.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ss_cache.o: $(SS_DIR)/ss_cache.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ss_checkpoints.o: $(SS_DIR)/ss_checkpoints.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(STORAGE_SERVER): $(COMMON_OBJ) $(SS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Storage Server built successfully"

# Build Client modules
$(BUILD_DIR)/client_main.o: $(CLIENT_DIR)/client_main.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/client_commands.o: $(CLIENT_DIR)/client_commands.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/client_network.o: $(CLIENT_DIR)/client_network.c $(INCLUDE_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ai_write.o: $(AI_WRITE_SRC) $(INCLUDE_DIR)/common.h $(INCLUDE_DIR)/ai_write.h
	$(CC) $(CFLAGS) -c $< -o $@

$(CLIENT): $(COMMON_OBJ) $(CLIENT_OBJ) $(AI_WRITE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(CLIENT_LDFLAGS)
	@echo "Client built successfully"

# Run Name Server
run-ns: $(NAME_SERVER)
	./$(NAME_SERVER)

# Run Storage Server (usage: make run-ss ID=1 PORT=9001)
ID ?= 1
PORT ?= 9001
run-ss: $(STORAGE_SERVER)
	./$(STORAGE_SERVER) $(ID) $(PORT)

# Run Client
run-client: $(CLIENT)
	./$(CLIENT)

# Run multiple storage servers in background
run-servers: $(NAME_SERVER) $(STORAGE_SERVER)
	@echo "Starting Name Server..."
	./$(NAME_SERVER) &
	@sleep 2
	@echo "Starting Storage Server 1..."
	./$(STORAGE_SERVER) 1 9001 &
	@sleep 1
	@echo "Starting Storage Server 2..."
	./$(STORAGE_SERVER) 2 9002 &
	@echo "All servers started. Use 'make stop-servers' to stop them."

# Stop all servers
stop-servers:
	@echo "Stopping all servers..."
	-@pkill -f name_server 2>/dev/null || true
	-@pkill -f storage_server 2>/dev/null || true
	@sleep 1
	@echo "All servers stopped."

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(NAME_SERVER) $(STORAGE_SERVER) $(CLIENT)
	@echo "Build artifacts cleaned"

# Clean everything (including logs and storage)
clean-all: clean
	rm -rf logs/*.log
	rm -rf storage/ss*
	@echo "All files cleaned"

# Create test files
test-files:
	@mkdir -p storage/ss1
	@echo "Hello world. This is a test file." > storage/ss1/test1.txt
	@echo "Another test file! How are you?" > storage/ss1/test2.txt
	@mkdir -p storage/ss2
	@echo "ls -la" > storage/ss2/commands.sh
	@echo "Test files created"

# Help
help:
	@echo "Network File System - Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all              - Build all components (default)"
	@echo "  name_server      - Build Name Server only"
	@echo "  storage_server   - Build Storage Server only"
	@echo "  client           - Build Client only"
	@echo "  run-ns           - Run Name Server"
	@echo "  run-ss           - Run Storage Server (use ID=<n> PORT=<port>)"
	@echo "  run-client       - Run Client"
	@echo "  run-servers      - Run Name Server and 2 Storage Servers"
	@echo "  stop-servers     - Stop all running servers"
	@echo "  test-files       - Create test files in storage"
	@echo "  clean            - Remove build artifacts"
	@echo "  clean-all        - Remove build artifacts, logs, and storage"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make"
	@echo "  make run-servers"
	@echo "  make run-client"
	@echo "  make run-ss ID=3 PORT=9003"

.PHONY: all directories clean clean-all run-ns run-ss run-client run-servers stop-servers test-files help
