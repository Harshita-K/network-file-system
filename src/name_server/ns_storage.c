#include "../../include/ns_state.h"
#include "../../include/ns_storage.h"
#include "../../include/ns_file_ops.h"
#include "../../include/ns_trie.h"

int register_storage_server(Packet *packet, const char *actual_ip)
{
    pthread_mutex_lock(&ns.ss_lock);

    if (ns.ss_count >= MAX_STORAGE_SERVERS)
    {
        pthread_mutex_unlock(&ns.ss_lock);
        return ERR_SERVER_UNAVAILABLE;
    }

    // Check if storage server with same ID already exists (reconnection)
    int is_reconnection = 0;
    for (int i = 0; i < ns.ss_count; i++)
    {
        if (ns.storage_servers[i].id == packet->ss_id)
        {
            // This is a reconnection - reactivate the server
            ns.storage_servers[i].is_active = 1;
            ns.storage_servers[i].last_heartbeat = time(NULL);
            // Update IP in case it changed (e.g., DHCP lease renewal)
            strncpy(ns.storage_servers[i].ip, actual_ip, INET_ADDRSTRLEN - 1);
            is_reconnection = 1;
            
            log_message("name_server", "INFO", "Storage Server %d reconnected and reactivated", packet->ss_id);
            
            // If this is an actual server coming back, update file metadata
            if (!ns.storage_servers[i].is_replica)
            {
                pthread_mutex_lock(&ns.file_lock);
                
                // Find all files that were failed over to replica
                int replica_id = ns.storage_servers[i].replica_id;
                for (int j = 0; j < ns.file_count; j++)
                {
                    // If file is on replica but should be on this actual server
                    if (ns.files[j].metadata.storage_server_id == replica_id)
                    {
                        // Trigger sync from replica to actual server
                        char *filename = strdup(ns.files[j].metadata.filename);
                        
                        // Move metadata back to actual server
                        ns.files[j].metadata.storage_server_id = packet->ss_id;
                        log_message("name_server", "INFO", "File %s moved back to actual server %d", 
                                   filename, packet->ss_id);
                        
                        // Spawn async thread to sync file content from replica to actual
                        pthread_t sync_tid;
                        Packet *sync_notify = malloc(sizeof(Packet));
                        if (sync_notify)
                        {
                            sync_notify->operation = OP_SYNC_METADATA;
                            sync_notify->ss_id = replica_id;  // Source: replica
                            sync_notify->flags = packet->ss_id;  // Destination: actual
                            strncpy(sync_notify->filename, filename, MAX_FILENAME - 1);
                            
                            if (pthread_create(&sync_tid, NULL, async_replicate_write, sync_notify) == 0)
                            {
                                pthread_detach(sync_tid);
                                log_message("name_server", "INFO", "Recovery sync started for %s from replica %d to actual %d", 
                                           filename, replica_id, packet->ss_id);
                            }
                            else
                            {
                                free(sync_notify);
                            }
                        }
                        
                        free(filename);
                    }
                }
                
                pthread_mutex_unlock(&ns.file_lock);
            }
            
            pthread_mutex_unlock(&ns.ss_lock);
            return SUCCESS;
        }
    }

    int idx = ns.ss_count++;
    StorageServerInfo *ss = &ns.storage_servers[idx];

    ss->id = packet->ss_id;
    strncpy(ss->ip, actual_ip, INET_ADDRSTRLEN - 1);  // Use actual IP from connection, not packet
    ss->client_port = packet->ss_port;
    ss->is_active = 1;
    ss->file_count = 0;
    ss->last_heartbeat = time(NULL);
    
    log_message("name_server", "INFO", "Storage Server %d registered from IP: %s (port: %d)", 
               ss->id, actual_ip, ss->client_port);
    
    // Replication pairing: Odd IDs are actual servers, Even IDs are replicas
    // SS1 (actual) <-> SS2 (replica)
    // SS3 (actual) <-> SS4 (replica), etc.
    if (packet->ss_id % 2 == 1)
    {
        // Odd ID: This is an actual server
        ss->is_replica = 0;
        ss->replica_id = packet->ss_id + 1;  // Replica will be next ID
        ss->actual_server_id = -1;
        log_message("name_server", "INFO", "Storage Server %d registered as ACTUAL server (replica: %d)", 
                   ss->id, ss->replica_id);
    }
    else
    {
        // Even ID: This is a replica server
        ss->is_replica = 1;
        ss->replica_id = -1;
        ss->actual_server_id = packet->ss_id - 1;  // Actual server is previous ID
        log_message("name_server", "INFO", "Storage Server %d registered as REPLICA server (actual: %d)", 
                   ss->id, ss->actual_server_id);
        
        // Update the actual server's replica_id pointer
        for (int i = 0; i < ns.ss_count - 1; i++)
        {
            if (ns.storage_servers[i].id == ss->actual_server_id)
            {
                ns.storage_servers[i].replica_id = ss->id;
                break;
            }
        }
    }

    pthread_mutex_unlock(&ns.ss_lock);

    // Parse file list
    char *token = strtok(packet->data, ",");
    while (token)
    {
        pthread_mutex_lock(&ns.file_lock);

        if (ns.file_count < MAX_FILES_PER_SS * MAX_STORAGE_SERVERS)
        {
            int file_idx = ns.file_count++;
            FileInfo *file = &ns.files[file_idx];

            strncpy(file->metadata.filename, token, MAX_FILENAME - 1);
            strcpy(file->metadata.owner, "system");
            file->metadata.storage_server_id = ss->id;
            file->metadata.created = time(NULL);
            file->metadata.last_accessed = time(NULL);
            file->metadata.version = 1;
            file->acl = NULL;
            pthread_mutex_init(&file->lock, NULL);
            
            // Sentence locks removed - storage server handles locking with pointer-based mutexes

            // Add to trie
            trie_insert(ns.file_trie, token, file_idx);

            ss->file_count++;
        }

        pthread_mutex_unlock(&ns.file_lock);
        token = strtok(NULL, ",");
    }

    log_message("name_server", "INFO", "Storage Server %d registered with %d files",
                ss->id, ss->file_count);

    return SUCCESS;
}

// Register client

void *async_replicate_create(void *arg)
{
    Packet *request = (Packet *)arg;
    int replica_id = request->ss_id;
    
    pthread_mutex_lock(&ns.ss_lock);
    
    // Find replica server
    StorageServerInfo *replica_ss = NULL;
    for (int i = 0; i < ns.ss_count; i++)
    {
        if (ns.storage_servers[i].id == replica_id)
        {
            replica_ss = &ns.storage_servers[i];
            break;
        }
    }
    
    if (!replica_ss || !replica_ss->is_active)
    {
        pthread_mutex_unlock(&ns.ss_lock);
        log_message("name_server", "WARNING", "Replica server %d not available for file %s", replica_id, request->filename);
        free(request);
        return NULL;
    }
    
    char replica_ip[INET_ADDRSTRLEN];
    strncpy(replica_ip, replica_ss->ip, INET_ADDRSTRLEN - 1);
    int replica_port = replica_ss->client_port;
    
    pthread_mutex_unlock(&ns.ss_lock);
    
    // Connect and send CREATE request to replica
    int replica_socket = connect_to_server(replica_ip, replica_port);
    if (replica_socket < 0)
    {
        log_message("name_server", "WARNING", "Failed to connect to replica server %d for file %s", replica_id, request->filename);
        free(request);
        return NULL;
    }
    
    send_packet(replica_socket, request);
    
    Packet response;
    receive_packet(replica_socket, &response);
    close(replica_socket);
    
    if (response.error_code == SUCCESS)
    {
        log_message("name_server", "INFO", "File %s replicated to server %d", request->filename, replica_id);
    }
    else
    {
        log_message("name_server", "WARNING", "Failed to replicate file %s to server %d: error %d", 
                   request->filename, replica_id, response.error_code);
    }
    
    free(request);
    return NULL;
}

// Async replication thread for WRITE operations

void *async_replicate_write(void *arg)
{
    Packet *notify = (Packet *)arg;
    char filename[MAX_FILENAME];
    strncpy(filename, notify->filename, MAX_FILENAME - 1);
    int source_ss_id = notify->ss_id;
    int dest_ss_id = notify->flags;  // If flags != 0, use it as destination (for recovery sync)
    
    pthread_mutex_lock(&ns.ss_lock);
    
    // Find the source storage server
    StorageServerInfo *source_ss = NULL;
    for (int i = 0; i < ns.ss_count; i++)
    {
        if (ns.storage_servers[i].id == source_ss_id)
        {
            source_ss = &ns.storage_servers[i];
            break;
        }
    }
    
    if (!source_ss)
    {
        pthread_mutex_unlock(&ns.ss_lock);
        log_message("name_server", "WARNING", "Source server %d not found for write replication of %s", 
                   source_ss_id, filename);
        free(notify);
        return NULL;
    }
    
    // Determine destination server ID
    int replica_id;
    if (dest_ss_id != 0)
    {
        // Recovery sync: explicit destination
        replica_id = dest_ss_id;
    }
    else
    {
        // Normal replication: determine replica from source
        replica_id = source_ss->is_replica ? source_ss->actual_server_id : source_ss->replica_id;
    }
    
    if (replica_id < 0)
    {
        pthread_mutex_unlock(&ns.ss_lock);
        log_message("name_server", "INFO", "No replica configured for server %d", source_ss_id);
        free(notify);
        return NULL;
    }
    
    // Find destination server
    StorageServerInfo *replica_ss = NULL;
    for (int i = 0; i < ns.ss_count; i++)
    {
        if (ns.storage_servers[i].id == replica_id)
        {
            replica_ss = &ns.storage_servers[i];
            break;
        }
    }
    
    if (!replica_ss || !replica_ss->is_active)
    {
        pthread_mutex_unlock(&ns.ss_lock);
        log_message("name_server", "WARNING", "Destination server %d not available for replication of %s", 
                   replica_id, filename);
        free(notify);
        return NULL;
    }
    
    char source_ip[INET_ADDRSTRLEN], replica_ip[INET_ADDRSTRLEN];
    strncpy(source_ip, source_ss->ip, INET_ADDRSTRLEN - 1);
    int source_port = source_ss->client_port;
    strncpy(replica_ip, replica_ss->ip, INET_ADDRSTRLEN - 1);
    int replica_port = replica_ss->client_port;
    
    pthread_mutex_unlock(&ns.ss_lock);
    
    // Step 1: Read file from source server
    int source_socket = connect_to_server(source_ip, source_port);
    if (source_socket < 0)
    {
        log_message("name_server", "WARNING", "Failed to connect to source server %d for replication of %s", 
                   source_ss_id, filename);
        free(notify);
        return NULL;
    }
    
    Packet read_request = {0};
    read_request.operation = OP_READ;
    strncpy(read_request.filename, filename, MAX_FILENAME - 1);
    strcpy(read_request.username, "system");
    
    send_packet(source_socket, &read_request);
    
    Packet read_response;
    receive_packet(source_socket, &read_response);
    close(source_socket);
    
    if (read_response.error_code != SUCCESS)
    {
        log_message("name_server", "WARNING", "Failed to read file %s from source server %d: error %d", 
                   filename, source_ss_id, read_response.error_code);
        free(notify);
        return NULL;
    }
    
    // Step 2: Send entire file content to replica using OP_SYNC_FILE
    int replica_socket = connect_to_server(replica_ip, replica_port);
    if (replica_socket < 0)
    {
        log_message("name_server", "WARNING", "Failed to connect to replica server %d for replication of %s", 
                   replica_id, filename);
        free(notify);
        return NULL;
    }
    
    Packet sync_request = {0};
    sync_request.operation = OP_SYNC_FILE;
    strncpy(sync_request.filename, filename, MAX_FILENAME - 1);
    strcpy(sync_request.username, "system");
    strncpy(sync_request.data, read_response.data, MAX_BUFFER_SIZE - 1);
    sync_request.data_len = strlen(read_response.data);
    
    send_packet(replica_socket, &sync_request);
    
    Packet sync_response;
    receive_packet(replica_socket, &sync_response);
    close(replica_socket);
    
    if (sync_response.error_code == SUCCESS)
    {
        log_message("name_server", "INFO", "Write replicated successfully: %s to server %d", 
                   filename, replica_id);
    }
    else
    {
        log_message("name_server", "WARNING", "Failed to sync file %s to replica server %d: error %d", 
                   filename, replica_id, sync_response.error_code);
    }
    
    free(notify);
    return NULL;
}

// Async checkpoint replication thread
void *async_replicate_checkpoint(void *arg)
{
    Packet *notify = (Packet *)arg;
    char filename[MAX_FILENAME];
    char tag[MAX_TAG_LENGTH];
    strncpy(filename, notify->filename, MAX_FILENAME - 1);
    strncpy(tag, notify->checkpoint_tag, MAX_TAG_LENGTH - 1);
    int source_ss_id = notify->ss_id;
    
    pthread_mutex_lock(&ns.ss_lock);
    
    // Find the source storage server
    StorageServerInfo *source_ss = NULL;
    for (int i = 0; i < ns.ss_count; i++)
    {
        if (ns.storage_servers[i].id == source_ss_id)
        {
            source_ss = &ns.storage_servers[i];
            break;
        }
    }
    
    if (!source_ss)
    {
        pthread_mutex_unlock(&ns.ss_lock);
        log_message("name_server", "WARNING", "Source server %d not found for checkpoint replication of %s", 
                   source_ss_id, filename);
        free(notify);
        return NULL;
    }
    
    // Determine replica server ID
    int replica_id = source_ss->is_replica ? source_ss->actual_server_id : source_ss->replica_id;
    
    if (replica_id < 0)
    {
        pthread_mutex_unlock(&ns.ss_lock);
        log_message("name_server", "INFO", "No replica configured for server %d", source_ss_id);
        free(notify);
        return NULL;
    }
    
    // Find replica server
    StorageServerInfo *replica_ss = NULL;
    for (int i = 0; i < ns.ss_count; i++)
    {
        if (ns.storage_servers[i].id == replica_id)
        {
            replica_ss = &ns.storage_servers[i];
            break;
        }
    }
    
    if (!replica_ss || !replica_ss->is_active)
    {
        pthread_mutex_unlock(&ns.ss_lock);
        log_message("name_server", "WARNING", "Replica server %d not available for checkpoint replication of %s", 
                   replica_id, filename);
        free(notify);
        return NULL;
    }
    
    char source_ip[INET_ADDRSTRLEN], replica_ip[INET_ADDRSTRLEN];
    strncpy(source_ip, source_ss->ip, INET_ADDRSTRLEN - 1);
    int source_port = source_ss->client_port;
    strncpy(replica_ip, replica_ss->ip, INET_ADDRSTRLEN - 1);
    int replica_port = replica_ss->client_port;
    
    pthread_mutex_unlock(&ns.ss_lock);
    
    // Step 1: Read checkpoint from source server
    int source_socket = connect_to_server(source_ip, source_port);
    if (source_socket < 0)
    {
        log_message("name_server", "WARNING", "Failed to connect to source server %d for checkpoint replication of %s", 
                   source_ss_id, filename);
        free(notify);
        return NULL;
    }
    
    Packet read_request = {0};
    read_request.operation = OP_VIEW_CHECKPOINT;
    strncpy(read_request.filename, filename, MAX_FILENAME - 1);
    strncpy(read_request.checkpoint_tag, tag, MAX_TAG_LENGTH - 1);
    strcpy(read_request.username, "system");
    
    send_packet(source_socket, &read_request);
    
    Packet read_response;
    receive_packet(source_socket, &read_response);
    close(source_socket);
    
    if (read_response.error_code != SUCCESS)
    {
        log_message("name_server", "WARNING", "Failed to read checkpoint %s:%s from source server %d: error %d", 
                   filename, tag, source_ss_id, read_response.error_code);
        free(notify);
        return NULL;
    }
    
    // Step 2: Save checkpoint to replica server
    int replica_socket = connect_to_server(replica_ip, replica_port);
    if (replica_socket < 0)
    {
        log_message("name_server", "WARNING", "Failed to connect to replica server %d for checkpoint replication of %s", 
                   replica_id, filename);
        free(notify);
        return NULL;
    }
    
    // First, ensure the base file exists on replica (may not if checkpoint is first operation)
    Packet sync_file_request = {0};
    sync_file_request.operation = OP_SYNC_FILE;
    strncpy(sync_file_request.filename, filename, MAX_FILENAME - 1);
    strcpy(sync_file_request.username, "system");
    strncpy(sync_file_request.data, read_response.data, MAX_BUFFER_SIZE - 1);
    sync_file_request.data_len = strlen(read_response.data);
    
    send_packet(replica_socket, &sync_file_request);
    
    Packet sync_file_response;
    receive_packet(replica_socket, &sync_file_response);
    
    // Now save the checkpoint
    Packet checkpoint_request = {0};
    checkpoint_request.operation = OP_CHECKPOINT;
    strncpy(checkpoint_request.filename, filename, MAX_FILENAME - 1);
    strncpy(checkpoint_request.checkpoint_tag, tag, MAX_TAG_LENGTH - 1);
    strcpy(checkpoint_request.username, "system");
    
    send_packet(replica_socket, &checkpoint_request);
    
    Packet checkpoint_response;
    receive_packet(replica_socket, &checkpoint_response);
    close(replica_socket);
    
    if (checkpoint_response.error_code == SUCCESS)
    {
        log_message("name_server", "INFO", "Checkpoint replicated successfully: %s:%s to server %d", 
                   filename, tag, replica_id);
    }
    else
    {
        log_message("name_server", "WARNING", "Failed to replicate checkpoint %s:%s to server %d: error %d", 
                   filename, tag, replica_id, checkpoint_response.error_code);
    }
    
    free(notify);
    return NULL;
}

// Handle CREATE operation
