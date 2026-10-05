#include "../../include/ns_state.h"
#include "../../include/ns_client.h"
#include "../../include/ns_file_ops.h"
#include "../../include/ns_storage.h"

int register_client(Packet *packet, int client_socket)
{
    pthread_mutex_lock(&ns.client_lock);

    if (ns.client_count >= MAX_CLIENTS)
    {
        pthread_mutex_unlock(&ns.client_lock);
        return ERR_SERVER_UNAVAILABLE;
    }

    // Check if client with same username already exists
    for (int i = 0; i < ns.client_count; i++)
    {
        if (strcmp(ns.clients[i].username, packet->username) == 0)
        {
            pthread_mutex_unlock(&ns.client_lock);
            log_message("name_server", "ERROR", "Client with username '%s' already connected", packet->username);
            return ERR_USER_ALREADY_LOGGED_IN;
        }
    }

    int idx = ns.client_count++;
    ClientInfo *client = &ns.clients[idx];

    strncpy(client->username, packet->username, MAX_USERNAME - 1);
    strncpy(client->ip, packet->data, INET_ADDRSTRLEN - 1);
    client->port = packet->ss_port;
    client->socket_fd = client_socket;
    client->connected_at = time(NULL);

    pthread_mutex_unlock(&ns.client_lock);

    log_message("name_server", "INFO", "Client registered: %s", client->username);

    return SUCCESS;
}

// Async replication thread for CREATE operations

void *handle_client_request(void *arg)
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

        log_request("name_server", client_ip, ntohs(addr.sin_port), "Client Request");

        response.operation = OP_ACK;
        response.error_code = SUCCESS;

        switch (request.operation)
        {
        case OP_REGISTER_SS:
            response.error_code = register_storage_server(&request, client_ip);
            break;

        case OP_REGISTER_CLIENT:
            response.error_code = register_client(&request, client_socket);
            break;

        case OP_CREATE:
            response.error_code = handle_create(&request, &response);
            break;

        case OP_DELETE:
            // Check if this is a delete notification from storage server (ss_id != 0)
            if (request.ss_id != 0)
            {
                // Storage server notifying us of a delete - replicate to replica
                int source_ss_id = request.ss_id;
                
                pthread_mutex_lock(&ns.ss_lock);
                
                StorageServerInfo *source_ss = NULL;
                for (int i = 0; i < ns.ss_count; i++)
                {
                    if (ns.storage_servers[i].id == source_ss_id)
                    {
                        source_ss = &ns.storage_servers[i];
                        break;
                    }
                }
                
                if (source_ss)
                {
                    int replica_id = source_ss->is_replica ? source_ss->actual_server_id : source_ss->replica_id;
                    
                    if (replica_id >= 0)
                    {
                        StorageServerInfo *replica_ss = NULL;
                        for (int i = 0; i < ns.ss_count; i++)
                        {
                            if (ns.storage_servers[i].id == replica_id)
                            {
                                replica_ss = &ns.storage_servers[i];
                                break;
                            }
                        }
                        
                        if (replica_ss && replica_ss->is_active)
                        {
                            // Forward delete to replica
                            int replica_socket = connect_to_server(replica_ss->ip, replica_ss->client_port);
                            if (replica_socket >= 0)
                            {
                                Packet delete_req = {0};
                                delete_req.operation = OP_DELETE;
                                strncpy(delete_req.filename, request.filename, MAX_FILENAME - 1);
                                strcpy(delete_req.username, "system");
                                
                                send_packet(replica_socket, &delete_req);
                                
                                Packet delete_resp;
                                receive_packet(replica_socket, &delete_resp);
                                close(replica_socket);
                                
                                if (delete_resp.error_code == SUCCESS)
                                {
                                    log_message("name_server", "INFO", "File %s deleted on replica server %d", 
                                               request.filename, replica_id);
                                }
                            }
                        }
                    }
                }
                
                pthread_mutex_unlock(&ns.ss_lock);
                response.error_code = SUCCESS;
            }
            else
            {
                // Normal delete from client
                response.error_code = handle_delete(&request, &response);
            }
            break;

        case OP_VIEW:
            response.error_code = handle_view(&request, &response);
            break;

        case OP_LIST_USERS:
            response.error_code = handle_list_users(&request, &response);
            break;

        case OP_ADD_ACCESS:
            response.error_code = handle_add_access(&request, &response);
            break;

        case OP_REM_ACCESS:
            response.error_code = handle_remove_access(&request, &response);
            break;

        case OP_INFO:
            response.error_code = handle_info(&request, &response);
            break;

        case OP_EXEC:
            response.error_code = handle_exec(&request, &response);
            break;

        case OP_UPDATE_METADATA:
            response.error_code = handle_update_metadata(&request, &response);
            break;

        case OP_ACQUIRE_LOCK:
        {
            // Check write permission before allowing lock acquisition
            if (!check_access(request.filename, request.username, ACCESS_WRITE))
            {
                response.error_code = ERR_PERMISSION_DENIED;
                break;
            }
            
            char lock_token[64];
            response.error_code = acquire_sentence_lock(request.filename, request.sentence_num,
                                                        request.username, lock_token);
            if (response.error_code == SUCCESS)
            {
                strncpy(response.lock_token, lock_token, 64);
                // Also return SS info for direct connection
                int ss_id = get_ss_for_file(request.filename);
                if (ss_id >= 0)
                {
                    StorageServerInfo *ss = NULL;
                    StorageServerInfo *replica_ss = NULL;
                    
                    pthread_mutex_lock(&ns.ss_lock);
                    
                    // Find primary storage server
                    for (int i = 0; i < ns.ss_count; i++)
                    {
                        if (ns.storage_servers[i].id == ss_id)
                        {
                            ss = &ns.storage_servers[i];
                            break;
                        }
                    }
                    
                    // Check if primary SS is inactive - use replica instead
                    if (ss && !ss->is_active)
                    {
                        log_message("name_server", "WARNING", "Storage Server %d is inactive for lock acquisition, attempting failover to replica", ss->id);
                        
                        int replica_id = ss->is_replica ? ss->actual_server_id : ss->replica_id;
                        
                        // Find replica server
                        if (replica_id >= 0)
                        {
                            for (int i = 0; i < ns.ss_count; i++)
                            {
                                if (ns.storage_servers[i].id == replica_id)
                                {
                                    replica_ss = &ns.storage_servers[i];
                                    break;
                                }
                            }
                            
                            if (replica_ss && replica_ss->is_active)
                            {
                                log_message("name_server", "INFO", "Failover successful: using replica server %d for lock on file %s", 
                                           replica_id, request.filename);
                                ss = replica_ss;  // Use replica instead
                                
                                // Update file metadata to point to the active replica
                                pthread_mutex_lock(&ns.file_lock);
                                int file_idx = find_file_index_locked(request.filename);
                                if (file_idx >= 0)
                                {
                                    ns.files[file_idx].metadata.storage_server_id = replica_id;
                                    log_message("name_server", "INFO", "File %s metadata updated to use server %d", 
                                               request.filename, replica_id);
                                }
                                pthread_mutex_unlock(&ns.file_lock);
                            }
                            else
                            {
                                pthread_mutex_unlock(&ns.ss_lock);
                                response.error_code = ERR_SERVER_UNAVAILABLE;
                                log_message("name_server", "ERROR", "Both primary and replica servers unavailable for file %s", 
                                           request.filename);
                                break;
                            }
                        }
                        else
                        {
                            pthread_mutex_unlock(&ns.ss_lock);
                            response.error_code = ERR_SERVER_UNAVAILABLE;
                            log_message("name_server", "ERROR", "Primary server %d inactive and no replica available", ss->id);
                            break;
                        }
                    }
                    
                    if (ss && ss->is_active)
                    {
                        // Proactive connectivity check before returning SS info
                        int test_socket = connect_to_server(ss->ip, ss->client_port);
                        if (test_socket < 0)
                        {
                            // Connection failed - mark as inactive and try replica
                            log_message("name_server", "WARNING", "Storage Server %d failed connectivity check, marking inactive", ss->id);
                            ss->is_active = 0;
                            
                            int replica_id = ss->is_replica ? ss->actual_server_id : ss->replica_id;
                            
                            if (replica_id >= 0)
                            {
                                replica_ss = NULL;
                                for (int i = 0; i < ns.ss_count; i++)
                                {
                                    if (ns.storage_servers[i].id == replica_id && ns.storage_servers[i].is_active)
                                    {
                                        replica_ss = &ns.storage_servers[i];
                                        break;
                                    }
                                }
                                
                                if (replica_ss)
                                {
                                    // Test replica connection too
                                    test_socket = connect_to_server(replica_ss->ip, replica_ss->client_port);
                                    if (test_socket >= 0)
                                    {
                                        close(test_socket);
                                        log_message("name_server", "INFO", "Failover to replica server %d successful", replica_id);
                                        ss = replica_ss;
                                        
                                        // Update file metadata
                                        pthread_mutex_lock(&ns.file_lock);
                                        int file_idx = find_file_index_locked(request.filename);
                                        if (file_idx >= 0)
                                        {
                                            ns.files[file_idx].metadata.storage_server_id = replica_id;
                                        }
                                        pthread_mutex_unlock(&ns.file_lock);
                                    }
                                    else
                                    {
                                        replica_ss->is_active = 0;
                                        ss = NULL;
                                    }
                                }
                                else
                                {
                                    ss = NULL;
                                }
                            }
                            else
                            {
                                ss = NULL;
                            }
                        }
                        else
                        {
                            close(test_socket);
                        }
                        
                        if (ss && ss->is_active)
                        {
                            strncpy(response.ss_ip, ss->ip, INET_ADDRSTRLEN);
                            response.ss_port = ss->client_port;
                            response.ss_id = ss->id;
                        }
                        else
                        {
                            response.error_code = ERR_SERVER_UNAVAILABLE;
                            log_message("name_server", "ERROR", "No active storage servers available for file %s", request.filename);
                        }
                    }
                    else if (response.error_code == SUCCESS)
                    {
                        response.error_code = ERR_SERVER_UNAVAILABLE;
                    }
                    
                    pthread_mutex_unlock(&ns.ss_lock);
                }
            }
            break;
        }

        case OP_RELEASE_LOCK:
            response.error_code = release_sentence_lock(request.filename, request.sentence_num,
                                                        request.lock_token);
            break;

        case OP_READ:
        case OP_WRITE:
        case OP_STREAM:
        case OP_UNDO:
        {
            // Return SS info for direct connection
            int ss_id = get_ss_for_file(request.filename);
            if (ss_id < 0)
            {
                response.error_code = ERR_FILE_NOT_FOUND;
            }
            else
            {
                // Check access
                int req_perm = (request.operation == OP_READ || request.operation == OP_STREAM) ? ACCESS_READ : ACCESS_WRITE;
                if (!check_access(request.filename, request.username, req_perm))
                {
                    response.error_code = ERR_PERMISSION_DENIED;
                }
                else
                {
                    StorageServerInfo *ss = NULL;
                    StorageServerInfo *replica_ss = NULL;
                    
                    pthread_mutex_lock(&ns.ss_lock);
                    
                    // Find primary storage server
                    for (int i = 0; i < ns.ss_count; i++)
                    {
                        if (ns.storage_servers[i].id == ss_id)
                        {
                            ss = &ns.storage_servers[i];
                            break;
                        }
                    }

                    // Check if primary SS is inactive - use replica instead
                    if (ss && !ss->is_active)
                    {
                        log_message("name_server", "WARNING", "Storage Server %d is inactive, attempting failover to replica", ss->id);
                        
                        int replica_id = ss->is_replica ? ss->actual_server_id : ss->replica_id;
                        
                        // Find replica server
                        if (replica_id >= 0)
                        {
                            for (int i = 0; i < ns.ss_count; i++)
                            {
                                if (ns.storage_servers[i].id == replica_id)
                                {
                                    replica_ss = &ns.storage_servers[i];
                                    break;
                                }
                            }
                            
                            if (replica_ss && replica_ss->is_active)
                            {
                                log_message("name_server", "INFO", "Failover successful: using replica server %d for file %s", 
                                           replica_id, request.filename);
                                ss = replica_ss;  // Use replica instead
                                
                                // Update file metadata to point to the active replica
                                pthread_mutex_lock(&ns.file_lock);
                                int file_idx = find_file_index_locked(request.filename);
                                if (file_idx >= 0)
                                {
                                    ns.files[file_idx].metadata.storage_server_id = replica_id;
                                    log_message("name_server", "INFO", "File %s metadata updated to use server %d", 
                                               request.filename, replica_id);
                                }
                                pthread_mutex_unlock(&ns.file_lock);
                            }
                            else
                            {
                                pthread_mutex_unlock(&ns.ss_lock);
                                response.error_code = ERR_SERVER_UNAVAILABLE;
                                log_message("name_server", "ERROR", "Both primary and replica servers unavailable for file %s", 
                                           request.filename);
                                break;
                            }
                        }
                        else
                        {
                            pthread_mutex_unlock(&ns.ss_lock);
                            response.error_code = ERR_SERVER_UNAVAILABLE;
                            log_message("name_server", "ERROR", "Primary server %d inactive and no replica available", ss->id);
                            break;
                        }
                    }

                    if (ss && ss->is_active)
                    {
                        response.ss_id = ss->id;
                        strncpy(response.ss_ip, ss->ip, INET_ADDRSTRLEN - 1);
                        response.ss_port = ss->client_port;
                    }
                    else
                    {
                        response.error_code = ERR_SERVER_UNAVAILABLE;
                    }
                    
                    pthread_mutex_unlock(&ns.ss_lock);
                }
            }
            break;
        }

        case OP_CREATE_FOLDER:
            response.error_code = handle_create_folder(&request, &response);
            break;

        case OP_MOVE_FILE:
            response.error_code = handle_move_file(&request, &response);
            break;

        case OP_VIEW_FOLDER:
            response.error_code = handle_view_folder(&request, &response);
            break;

        case OP_REQUEST_ACCESS:
            response.error_code = handle_request_access(&request, &response);
            break;

        case OP_LIST_REQUESTS:
            response.error_code = handle_list_requests(&request, &response);
            break;

        case OP_APPROVE_REQUEST:
            response.error_code = handle_approve_request(&request, &response);
            break;

        case OP_DENY_REQUEST:
            response.error_code = handle_deny_request(&request, &response);
            break;

        case OP_REPLICA_WRITE:
        {
            // Asynchronous write replication request from storage server
            pthread_t replica_tid;
            Packet *notify = malloc(sizeof(Packet));
            if (notify)
            {
                memcpy(notify, &request, sizeof(Packet));
                
                // Start async replication thread
                if (pthread_create(&replica_tid, NULL, async_replicate_write, notify) == 0)
                {
                    pthread_detach(replica_tid);
                    log_message("name_server", "INFO", "Write replication started for %s from server %d", 
                               request.filename, request.ss_id);
                }
                else
                {
                    free(notify);
                    log_message("name_server", "WARNING", "Failed to start write replication thread for %s", 
                               request.filename);
                }
            }
            response.error_code = SUCCESS;  // Always return success (fire and forget)
            break;
        }

        case OP_REPLICA_CHECKPOINT:
        {
            // Asynchronous checkpoint replication request from storage server
            pthread_t replica_tid;
            Packet *notify = malloc(sizeof(Packet));
            if (notify)
            {
                memcpy(notify, &request, sizeof(Packet));
                
                // Start async checkpoint replication thread
                if (pthread_create(&replica_tid, NULL, async_replicate_checkpoint, notify) == 0)
                {
                    pthread_detach(replica_tid);
                    log_message("name_server", "INFO", "Checkpoint replication started for %s:%s from server %d", 
                               request.filename, request.checkpoint_tag, request.ss_id);
                }
                else
                {
                    free(notify);
                    log_message("name_server", "WARNING", "Failed to start checkpoint replication thread for %s:%s", 
                               request.filename, request.checkpoint_tag);
                }
            }
            response.error_code = SUCCESS;  // Always return success (fire and forget)
            break;
        }

        case OP_HEARTBEAT:
        {
            // Storage server sending heartbeat to name server
            pthread_mutex_lock(&ns.ss_lock);
            
            int found = 0;
            for (int i = 0; i < ns.ss_count; i++)
            {
                if (ns.storage_servers[i].id == request.ss_id)
                {
                    // Update last heartbeat time
                    ns.storage_servers[i].last_heartbeat = time(NULL);
                    
                    // If server was inactive, reactivate it
                    if (!ns.storage_servers[i].is_active)
                    {
                        ns.storage_servers[i].is_active = 1;
                        log_message("name_server", "INFO", "Storage Server %d reconnected and reactivated", request.ss_id);
                        printf("Storage Server %d is now ACTIVE\n", request.ss_id);
                    }
                    
                    found = 1;
                    // log_message("name_server", "DEBUG", "Heartbeat received from Storage Server %d", request.ss_id);
                    break;
                }
            }
            
            pthread_mutex_unlock(&ns.ss_lock);
            
            response.operation = OP_ACK;
            response.error_code = found ? SUCCESS : ERR_SERVER_UNAVAILABLE;
            break;
        }

        default:
            response.error_code = ERR_INVALID_OPERATION;
            break;
        }

        send_packet(client_socket, &response);
        log_response("name_server", client_ip, ntohs(addr.sin_port), response.error_code);
    }

    close(client_socket);
    return NULL;
}

