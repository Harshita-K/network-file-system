#define _DEFAULT_SOURCE
#include "../../include/ss_state.h"
#include "../../include/ss_file_ops.h"
#include "../../include/ss_metadata.h"
#include "../../include/ss_cache.h"
#include "../../include/ss_checkpoints.h"
#include <sys/time.h>
#include <dirent.h>

// Global storage server state
StorageServer ss;

void init_storage_server(int id)
{
    ss.id = id;
    snprintf(ss.base_path, sizeof(ss.base_path), "storage/ss%d", id);
    ss.is_running = 1;
    ss.undo_count = 0;

    // Create storage directory
    mkdir("storage", 0777);
    mkdir(ss.base_path, 0777);

    // Initialize locks
    for (int i = 0; i < MAX_FILES_PER_SS; i++)
    {
        pthread_rwlock_init(&ss.file_locks[i], NULL); // Initialize rwlock
        ss.undo_top[i] = -1;
    }
    
    // Initialize shared file cache
    pthread_mutex_init(&ss.cache_mgmt_lock, NULL);
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        ss.file_cache[i].valid = 0;
        ss.file_cache[i].file_structure = NULL;
        ss.file_cache[i].active_sessions = 0;
        pthread_mutex_init(&ss.file_cache[i].cache_lock, NULL);
    }
    
    // Initialize write sessions
    pthread_mutex_init(&ss.sessions_lock, NULL);
    for (int i = 0; i < MAX_ACTIVE_WRITES; i++)
    {
        ss.write_sessions[i].active = 0;
        ss.write_sessions[i].locked_sentence = NULL;
        ss.write_sessions[i].working_copy = NULL;  // Initialize working copy
        ss.write_sessions[i].cache_index = -1;
    }

    log_message("storage_server", "INFO", "Storage Server %d initialized at %s", id, ss.base_path);
}

// Get file path
void get_file_path(const char *filename, char *filepath)
{
    snprintf(filepath, MAX_PATH, "%s/%s", ss.base_path, filename);
}

// Get metadata database file path
void get_metadata_db_path(char *dbpath)
{
    snprintf(dbpath, MAX_PATH, "%s/metadata.db", ss.base_path);
}

// Save metadata to database file

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(client_socket, (struct sockaddr *)&addr, &addr_len);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    Packet request, response;

    while (1)
    {
        memset(&request, 0, sizeof(Packet));
        memset(&response, 0, sizeof(Packet));

        if (receive_packet(client_socket, &request) <= 0)
        {
            break;
        }

        // Determine operation name for logging
        const char *op_name = "Unknown";
        switch (request.operation)
        {
        case OP_READ:
            op_name = "READ";
            break;
        case OP_STREAM:
            op_name = "STREAM";
            break;
        case OP_CREATE:
            op_name = "CREATE";
            break;
        case OP_DELETE:
            op_name = "DELETE";
            break;
        case OP_WRITE:
            op_name = "WRITE";
            break;
        case OP_INFO:
            op_name = "INFO";
            break;
        case OP_UNDO:
            op_name = "UNDO";
            break;
        }
        log_request("storage_server", client_ip, ntohs(addr.sin_port), op_name);

        response.operation = OP_ACK;
        response.error_code = SUCCESS;

        switch (request.operation)
        {
        case OP_CREATE:
            response.error_code = handle_create_file(request.filename, request.username);
            send_packet(client_socket, &response);
            break;

        case OP_DELETE:
            response.error_code = handle_delete_file(request.filename);
            send_packet(client_socket, &response);
            break;

        case OP_READ:
            response.error_code = handle_read_file(request.filename, response.data);
            response.data_len = strlen(response.data);
            send_packet(client_socket, &response);
            break;

        case OP_WRITE:
            response.error_code = handle_write_file(request.filename, request.sentence_num,
                                                    request.word_index, request.data, request.lock_token);
            send_packet(client_socket, &response);
            break;

        case OP_RELEASE_LOCK:
            // Client finished writing (ETIRW) - close the write session
            {
                ActiveWriteSession* session = find_write_session(request.filename, request.sentence_num, request.lock_token);
                if (session)
                {
                    close_write_session(session);
                    response.error_code = SUCCESS;
                }
                else
                {
                    response.error_code = ERR_FILE_NOT_FOUND; // No active session found
                }
                send_packet(client_socket, &response);
            }
            break;

        case OP_UNDO:
            response.error_code = handle_undo(request.filename);
            send_packet(client_socket, &response);
            break;

        case OP_INFO:
            response.error_code = handle_file_info(request.filename, &response);
            send_packet(client_socket, &response);
            break;

        case OP_STREAM:
            handle_stream_file(client_socket, request.filename);
            break;

        case OP_CHECKPOINT:
            response.error_code = handle_save_checkpoint(request.filename, request.checkpoint_tag);
            send_packet(client_socket, &response);
            break;

        case OP_VIEW_CHECKPOINT:
            response.error_code = handle_view_checkpoint(request.filename, request.checkpoint_tag, response.data);
            response.data_len = strlen(response.data);
            send_packet(client_socket, &response);
            break;

        case OP_REVERT_CHECKPOINT:
            response.error_code = handle_revert_checkpoint(request.filename, request.checkpoint_tag);
            send_packet(client_socket, &response);
            break;

        case OP_LIST_CHECKPOINTS:
            response.error_code = handle_list_checkpoints(request.filename, response.data);
            response.data_len = strlen(response.data);
            send_packet(client_socket, &response);
            break;

        case OP_HEARTBEAT:
            // Respond to heartbeat from name server
            response.operation = OP_ACK;
            response.error_code = SUCCESS;
            send_packet(client_socket, &response);
            // log_message("storage_server", "DEBUG", "Heartbeat response sent");
            break;

        case OP_SYNC_FILE:
        {
            // Direct file sync from name server (used for replication)
            // Parse the text content and save it directly
            char filepath[MAX_PATH];
            get_file_path(request.filename, filepath);
            
            // Parse text into file structure
            FileStructure *file = parse_text_to_file(request.data);
            if (!file)
            {
                response.error_code = ERR_MEMORY_ERROR;
                send_packet(client_socket, &response);
                break;
            }
            
            // Save directly to disk
            int result = save_file_to_disk(request.filename, file);
            free_file_structure(file);
            
            if (result == SUCCESS)
            {
                // Update metadata
                increment_file_version(request.filename, request.username);
                log_message("storage_server", "INFO", "File synced from replication: %s", request.filename);
            }
            
            response.error_code = result;
            send_packet(client_socket, &response);
            break;
        }

        default:
            response.error_code = ERR_INVALID_OPERATION;
            send_packet(client_socket, &response);
            break;
        }

        log_response("storage_server", client_ip, ntohs(addr.sin_port), response.error_code);
    }

    close(client_socket);
    return NULL;
}

// Client server thread
void *client_server_thread(void *arg)
{
    int port = *(int *)arg;
    free(arg);

    ss.client_server_socket = create_server_socket(port);
    if (ss.client_server_socket < 0)
    {
        log_message("storage_server", "ERROR", "Failed to create client server socket");
        return NULL;
    }

    log_message("storage_server", "INFO", "Client server listening on port %d", port);

    while (ss.is_running)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int *client_socket = malloc(sizeof(int));
        *client_socket = accept(ss.client_server_socket, (struct sockaddr *)&client_addr, &addr_len);

        if (*client_socket < 0)
        {
            free(client_socket);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_socket);
        pthread_detach(tid);
    }

    return NULL;
}

// List all files in storage
void list_files(char files[][MAX_FILENAME], int *count)
{
    *count = 0;
    DIR *dir = opendir(ss.base_path);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < MAX_FILES_PER_SS)
    {
        if (entry->d_type == DT_REG)
        {
            strncpy(files[*count], entry->d_name, MAX_FILENAME - 1);
            (*count)++;
        }
    }

    closedir(dir);
}

// Register with Name Server
int register_with_nm(int nm_port, int client_port)
{
    ss.nm_socket = connect_to_server(NAME_SERVER_IP, NAME_SERVER_PORT);
    if (ss.nm_socket < 0)
    {
        log_message("storage_server", "ERROR", "Failed to connect to Name Server");
        return -1;
    }

    // Send registration packet
    Packet reg_packet = {0};
    reg_packet.operation = OP_REGISTER_SS;
    reg_packet.ss_id = ss.id;
    snprintf(reg_packet.ss_ip, sizeof(reg_packet.ss_ip), "127.0.0.1");
    reg_packet.ss_port = client_port;

    // List files
    char files[MAX_FILES_PER_SS][MAX_FILENAME];
    int file_count;
    list_files(files, &file_count);

    // Send file list as data
    char file_list[MAX_BUFFER_SIZE] = "";
    for (int i = 0; i < file_count; i++)
    {
        strcat(file_list, files[i]);
        if (i < file_count - 1)
            strcat(file_list, ",");
    }
    strncpy(reg_packet.data, file_list, MAX_BUFFER_SIZE - 1);

    send_packet(ss.nm_socket, &reg_packet);

    // Wait for acknowledgment
    Packet ack;
    receive_packet(ss.nm_socket, &ack);

    if (ack.error_code == SUCCESS)
    {
        log_message("storage_server", "INFO", "Registered with Name Server");
        return 0;
    }

    return -1;
}

// Heartbeat sender thread - actively sends heartbeats to name server
void *heartbeat_sender_thread(void *arg)
{
    log_message("storage_server", "INFO", "Heartbeat sender thread started");
    
    while (ss.is_running)
    {
        sleep(HEARTBEAT_INTERVAL);
        
        // Create heartbeat packet
        Packet heartbeat = {0};
        heartbeat.operation = OP_HEARTBEAT;
        heartbeat.ss_id = ss.id;
        
        // Send heartbeat to name server
        if (send_packet(ss.nm_socket, &heartbeat) < 0)
        {
            log_message("storage_server", "WARNING", "Failed to send heartbeat to name server");
            continue;
        }
        
        // Wait for acknowledgment (with timeout)
        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        setsockopt(ss.nm_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        
        Packet response = {0};
        if (receive_packet(ss.nm_socket, &response) < 0)
        {
            log_message("storage_server", "WARNING", "No heartbeat acknowledgment from name server");
        }
        else
        {
            // log_message("storage_server", "DEBUG", "Heartbeat acknowledged by name server");
        }
    }
    
    log_message("storage_server", "INFO", "Heartbeat sender thread stopped");
    return NULL;
}

// Main function
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <server_id> <client_port>\n", argv[0]);
        return 1;
    }

    int server_id = atoi(argv[1]);
    int client_port = atoi(argv[2]);

    init_storage_server(server_id);

    // Start client server thread
    pthread_t client_thread;
    int *port_arg = malloc(sizeof(int));
    *port_arg = client_port;
    pthread_create(&client_thread, NULL, client_server_thread, port_arg);

    // Give client server time to start
    sleep(1);

    // Register with Name Server
    if (register_with_nm(NAME_SERVER_PORT, client_port) < 0)
    {
        return 1;
    }
    
    // Start heartbeat sender thread
    pthread_t heartbeat_thread;
    pthread_create(&heartbeat_thread, NULL, heartbeat_sender_thread, NULL);
    
    pthread_join(client_thread, NULL);
    // Cleanup
    ss.is_running = 0;
    close(ss.nm_socket);
    close(ss.client_server_socket);

    return 0;
}
