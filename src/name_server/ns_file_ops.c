#include "../../include/ns_state.h"
#include "../../include/ns_file_ops.h"
#include "../../include/ns_trie.h"
#include "../../include/ns_storage.h"

void generate_lock_token(char *token, const char *filename, int sentence_num, const char *username)
{
    snprintf(token, 64, "%s_%d_%s_%ld", filename, sentence_num, username, time(NULL));
}

// Acquire sentence lock - Just generate token and return storage server info
// Client will connect to storage server directly to acquire the lock
int acquire_sentence_lock(const char *filename, int sentence_num, const char *username, char *lock_token)
{
    // Generate a lock token
    generate_lock_token(lock_token, filename, sentence_num, username);
    
    log_message("name_server", "INFO", "Lock token generated: %s sentence %d by %s (token: %s)",
                filename, sentence_num, username, lock_token);

    return SUCCESS;
}

// Release sentence lock - REMOVED: Storage server handles unlocking
// This is now a no-op for backwards compatibility
int release_sentence_lock(const char *filename, int sentence_num, const char *lock_token)
{
    log_message("name_server", "INFO", "Lock release requested: %s sentence %d (token: %s)",
                filename, sentence_num, lock_token);

    return SUCCESS;
}

// Find file by name (O(m) where m is filename length)
// NOTE: Caller must hold ns.file_lock!
int find_file_index_locked(const char *filename)
{
    return trie_search(ns.file_trie, filename);
}

// Thread-safe wrapper that acquires lock
int find_file_index(const char *filename)
{
    pthread_mutex_lock(&ns.file_lock);
    int idx = trie_search(ns.file_trie, filename);
    pthread_mutex_unlock(&ns.file_lock);
    return idx;
}

// Get storage server for file
int get_ss_for_file(const char *filename)
{
    // Check cache first
    int cached_ss = cache_get(filename);
    if (cached_ss >= 0)
    {
        log_message("name_server", "DEBUG", "Cache hit for file: %s", filename);
        return cached_ss;
    }

    // Search in trie
    int file_idx = find_file_index(filename);
    if (file_idx < 0)
        return -1;

    int ss_id = ns.files[file_idx].metadata.storage_server_id;

    // Update cache
    cache_put(filename, ss_id);

    return ss_id;
}

// Check user access
int check_access(const char *filename, const char *username, int required_permission)
{
    int file_idx = find_file_index_locked(filename); // Use locked version - caller holds lock
    if (file_idx < 0)
        return 0;

    FileInfo *file = &ns.files[file_idx];

    // Owner has all permissions
    if (strcmp(file->metadata.owner, username) == 0)
    {
        return 1;
    }

    // Check ACL
    AccessEntry *entry = file->acl;
    while (entry)
    {
        if (strcmp(entry->username, username) == 0)
        {
            if (required_permission == ACCESS_READ)
            {
                return (entry->permissions >= ACCESS_READ);
            }
            else if (required_permission == ACCESS_WRITE)
            {
                return (entry->permissions >= ACCESS_WRITE);
            }
        }
        entry = entry->next;
    }

    return 0;
}

// Get user's folder path for a file (returns "" if not set)
const char* get_user_folder_path(FileInfo *file, const char *username)
{
    UserFolderPath *ufp = file->user_folders;
    while (ufp)
    {
        if (strcmp(ufp->username, username) == 0)
        {
            return ufp->folder_path;
        }
        ufp = ufp->next;
    }
    return ""; // Default to root if not set
}

// Set user's folder path for a file
void set_user_folder_path(FileInfo *file, const char *username, const char *folder_path)
{
    // Check if user already has a folder path set
    UserFolderPath *ufp = file->user_folders;
    while (ufp)
    {
        if (strcmp(ufp->username, username) == 0)
        {
            // Update existing entry
            strncpy(ufp->folder_path, folder_path, MAX_PATH - 1);
            ufp->folder_path[MAX_PATH - 1] = '\0';
            return;
        }
        ufp = ufp->next;
    }
    
    // Create new entry
    UserFolderPath *new_ufp = (UserFolderPath *)malloc(sizeof(UserFolderPath));
    if (new_ufp)
    {
        strncpy(new_ufp->username, username, MAX_USERNAME - 1);
        new_ufp->username[MAX_USERNAME - 1] = '\0';
        strncpy(new_ufp->folder_path, folder_path, MAX_PATH - 1);
        new_ufp->folder_path[MAX_PATH - 1] = '\0';
        new_ufp->next = file->user_folders;
        file->user_folders = new_ufp;
    }
}

// Check if user exists (client is registered)
int user_exists(const char *username)
{
    pthread_mutex_lock(&ns.client_lock);
    
    for (int i = 0; i < ns.client_count; i++)
    {
        if (strcmp(ns.clients[i].username, username) == 0)
        {
            pthread_mutex_unlock(&ns.client_lock);
            return 1;
        }
    }
    
    pthread_mutex_unlock(&ns.client_lock);
    return 0;
}

// Register storage server

int handle_create(Packet *request, Packet *response)
{
    // Check for reserved extensions
    const char *filename = request->filename;
    size_t len = strlen(filename);
    
    if ((len > 3 && strcmp(filename + len - 3, ".db") == 0) ||
        (len > 11 && strcmp(filename + len - 11, ".checkpoint") == 0))
    {
        log_message("name_server", "WARNING", "Attempt to create file with reserved extension: %s", filename);
        return ERR_RESERVED_EXTENSION;
    }
    
    // Check if file already exists
    if (find_file_index(request->filename) >= 0)
    {
        return ERR_FILE_EXISTS;
    }

    // Select storage server (round-robin) - only from active, non-replica servers
    pthread_mutex_lock(&ns.ss_lock);
    if (ns.ss_count == 0)
    {
        pthread_mutex_unlock(&ns.ss_lock);
        return ERR_SERVER_UNAVAILABLE;
    }

    // Find active actual servers (not replicas)
    StorageServerInfo *selected_ss = NULL;
    static int ss_round_robin = 0;
    int attempts = 0;
    
    while (attempts < ns.ss_count && !selected_ss)
    {
        int ss_idx = ss_round_robin % ns.ss_count;
        ss_round_robin++;
        
        StorageServerInfo *ss = &ns.storage_servers[ss_idx];
        
        // Select only if active and not a replica
        if (ss->is_active && !ss->is_replica)
        {
            selected_ss = ss;
        }
        
        attempts++;
    }
    
    if (!selected_ss)
    {
        pthread_mutex_unlock(&ns.ss_lock);
        log_message("name_server", "ERROR", "No active storage servers available for CREATE");
        return ERR_SERVER_UNAVAILABLE;
    }
    
    int ss_id = selected_ss->id;
    char ss_ip[INET_ADDRSTRLEN];
    strncpy(ss_ip, selected_ss->ip, INET_ADDRSTRLEN - 1);
    int ss_port = selected_ss->client_port;
    int replica_id = selected_ss->replica_id;
    
    pthread_mutex_unlock(&ns.ss_lock);

    // Forward request to storage server
    int ss_socket = connect_to_server(ss_ip, ss_port);
    if (ss_socket < 0)
    {
        return ERR_SERVER_UNAVAILABLE;
    }

    send_packet(ss_socket, request);

    Packet ss_response;
    receive_packet(ss_socket, &ss_response);
    close(ss_socket);

    if (ss_response.error_code == SUCCESS)
    {
        // Add file to our records
        pthread_mutex_lock(&ns.file_lock);

        if (ns.file_count < MAX_FILES_PER_SS * MAX_STORAGE_SERVERS)
        {
            int file_idx = ns.file_count++;
            FileInfo *file = &ns.files[file_idx];

            strncpy(file->metadata.filename, request->filename, MAX_FILENAME - 1);
            strncpy(file->metadata.owner, request->username, MAX_USERNAME - 1);
            file->metadata.storage_server_id = ss_id;
            file->metadata.created = time(NULL);
            file->metadata.last_accessed = time(NULL);
            file->metadata.last_modified = time(NULL);
            file->metadata.version = 1;
            file->acl = NULL;
            file->user_folders = NULL; // Initialize per-user folder paths
            pthread_mutex_init(&file->lock, NULL);
            
            // Set owner's default folder path to root
            set_user_folder_path(file, request->username, "");
            
            // Sentence locks removed - storage server handles locking with pointer-based mutexes

            // Add to trie
            trie_insert(ns.file_trie, request->filename, file_idx);
        }

        pthread_mutex_unlock(&ns.file_lock);
        
        // Asynchronously replicate to replica server if exists
        if (replica_id >= 0)
        {
            pthread_t replica_tid;
            Packet *replica_request = malloc(sizeof(Packet));
            if (replica_request)
            {
                memcpy(replica_request, request, sizeof(Packet));
                replica_request->ss_id = replica_id;
                
                // Start async replication thread
                if (pthread_create(&replica_tid, NULL, async_replicate_create, replica_request) == 0)
                {
                    pthread_detach(replica_tid);
                }
                else
                {
                    free(replica_request);
                    log_message("name_server", "WARNING", "Failed to start replication thread for file %s", request->filename);
                }
            }
        }
    }

    return ss_response.error_code;
}

// Handle DELETE operation
int handle_delete(Packet *request, Packet *response)
{
    // Check if file exists and user has permission
    if (!check_access(request->filename, request->username, ACCESS_WRITE))
    {
        int file_idx = find_file_index(request->filename);
        if (file_idx < 0)
            return ERR_FILE_NOT_FOUND;
        if (strcmp(ns.files[file_idx].metadata.owner, request->username) != 0)
        {
            return ERR_PERMISSION_DENIED;
        }
    }

    int ss_id = get_ss_for_file(request->filename);
    if (ss_id < 0)
        return ERR_FILE_NOT_FOUND;

    // Find SS
    StorageServerInfo *ss = NULL;
    for (int i = 0; i < ns.ss_count; i++)
    {
        if (ns.storage_servers[i].id == ss_id)
        {
            ss = &ns.storage_servers[i];
            break;
        }
    }

    if (!ss)
        return ERR_SERVER_UNAVAILABLE;

    // Forward to SS
    int ss_socket = connect_to_server(ss->ip, ss->client_port);
    if (ss_socket < 0)
        return ERR_SERVER_UNAVAILABLE;

    send_packet(ss_socket, request);

    Packet ss_response;
    receive_packet(ss_socket, &ss_response);
    close(ss_socket);

    // If deletion was successful, remove from name server's registry
    if (ss_response.error_code == SUCCESS)
    {
        pthread_mutex_lock(&ns.file_lock);
        
        int file_idx = find_file_index_locked(request->filename);
        if (file_idx >= 0)
        {
            FileInfo *file = &ns.files[file_idx];
            
            // Free ACL entries
            AccessEntry *entry = file->acl;
            while (entry)
            {
                AccessEntry *next = entry->next;
                free(entry);
                entry = next;
            }
            
            // Sentence locks removed - no need to free anything
            
            // Remove from trie by marking as invalid
            // Note: We can't easily remove from trie, so we'll shift the array
            // For simplicity, we'll mark the file as deleted by clearing the filename
            // A better approach would be to rebuild the trie, but for now:
            
            // Shift all files after this one down by one position
            for (int i = file_idx; i < ns.file_count - 1; i++)
            {
                ns.files[i] = ns.files[i + 1];
            }
            ns.file_count--;
            
            // Rebuild the trie since indices have changed
            free_trie(ns.file_trie);
            ns.file_trie = create_trie_node();
            for (int i = 0; i < ns.file_count; i++)
            {
                trie_insert(ns.file_trie, ns.files[i].metadata.filename, i);
            }
            
            log_message("name_server", "INFO", "File removed from registry: %s", request->filename);
        }
        
        pthread_mutex_unlock(&ns.file_lock);
    }

    // Invalidate cache
    cache_invalidate(request->filename);

    return ss_response.error_code;
}

// Handle VIEW operation
int handle_view(Packet *request, Packet *response)
{
    char result[MAX_BUFFER_SIZE] = "";
    int count = 0;

    pthread_mutex_lock(&ns.file_lock);
    // printf("file count: %d\n", ns.file_count);
    for (int i = 0; i < ns.file_count; i++)
    {
        FileInfo *file = &ns.files[i];

        // Check flags
        int show_all = (request->flags & 1);     // -a flag
        int show_details = (request->flags & 2); // -l flag
        // Check access
        if (!show_all && !check_access(file->metadata.filename, request->username, ACCESS_READ))
        {
            continue;
        }

        if (show_details)
        {
            char line[512];
            snprintf(line, sizeof(line), "%s\t%s\t%d words\t%d chars\t%s\n",
                     file->metadata.filename, file->metadata.owner,
                     file->metadata.word_count, file->metadata.char_count,
                     format_time(file->metadata.last_accessed));
            strcat(result, line);
        }
        else
        {
            strcat(result, file->metadata.filename);
            strcat(result, "\n");
        }
        count++;
    }

    pthread_mutex_unlock(&ns.file_lock);

    if (count == 0)
    {
        strcpy(result, "No files found\n");
    }

    strncpy(response->data, result, MAX_BUFFER_SIZE - 1);
    response->data_len = strlen(result);

    return SUCCESS;
}

// Handle LIST operation (list users)
int handle_list_users(Packet *request, Packet *response)
{
    char result[MAX_BUFFER_SIZE] = "";

    pthread_mutex_lock(&ns.client_lock);

    for (int i = 0; i < ns.client_count; i++)
    {
        strcat(result, ns.clients[i].username);
        strcat(result, "\n");
    }

    pthread_mutex_unlock(&ns.client_lock);

    strncpy(response->data, result, MAX_BUFFER_SIZE - 1);
    response->data_len = strlen(result);

    return SUCCESS;
}

// Handle ADD ACCESS operation
int handle_add_access(Packet *request, Packet *response)
{
    int file_idx = find_file_index(request->filename);
    if (file_idx < 0)
        return ERR_FILE_NOT_FOUND;

    FileInfo *file = &ns.files[file_idx];

    // Only owner can add access
    if (strcmp(file->metadata.owner, request->username) != 0)
    {
        return ERR_PERMISSION_DENIED;
    }

    // Parse target username from data
    char target_user[MAX_USERNAME];
    strncpy(target_user, request->data, MAX_USERNAME - 1);
    target_user[MAX_USERNAME - 1] = '\0';

    // Check if target user exists
    if (!user_exists(target_user))
    {
        log_message("name_server", "ERROR", "User '%s' not found", target_user);
        return ERR_USER_NOT_FOUND;
    }

    // Add to ACL
    AccessEntry *new_entry = (AccessEntry *)malloc(sizeof(AccessEntry));
    strncpy(new_entry->username, target_user, MAX_USERNAME - 1);
    new_entry->permissions = request->flags; // ACCESS_READ or ACCESS_WRITE
    new_entry->next = file->acl;
    file->acl = new_entry;
    
    // Initialize folder path for the new user (default to root)
    set_user_folder_path(file, target_user, "");

    log_message("name_server", "INFO", "Access granted: %s -> %s (%d)",
                request->filename, target_user, request->flags);

    return SUCCESS;
}

// Handle REMOVE ACCESS operation
int handle_remove_access(Packet *request, Packet *response)
{
    int file_idx = find_file_index(request->filename);
    if (file_idx < 0)
        return ERR_FILE_NOT_FOUND;

    FileInfo *file = &ns.files[file_idx];

    // Only owner can remove access
    if (strcmp(file->metadata.owner, request->username) != 0)
    {
        return ERR_PERMISSION_DENIED;
    }

    // Parse target username from data
    char target_user[MAX_USERNAME];
    strncpy(target_user, request->data, MAX_USERNAME - 1);
    target_user[MAX_USERNAME - 1] = '\0';

    // Check if target user exists
    if (!user_exists(target_user))
    {
        log_message("name_server", "ERROR", "User '%s' not found", target_user);
        return ERR_USER_NOT_FOUND;
    }

    // Remove from ACL
    AccessEntry *prev = NULL;
    AccessEntry *curr = file->acl;

    while (curr)
    {
        if (strcmp(curr->username, target_user) == 0)
        {
            if (prev)
            {
                prev->next = curr->next;
            }
            else
            {
                file->acl = curr->next;
            }
            free(curr);
            log_message("name_server", "INFO", "Access removed: %s -> %s",
                        request->filename, target_user);
            return SUCCESS;
        }
        prev = curr;
        curr = curr->next;
    }

    return ERR_INVALID_OPERATION;
}

// Handle INFO operation
int handle_info(Packet *request, Packet *response)
{
    if (!check_access(request->filename, request->username, ACCESS_READ))
    {
        return ERR_PERMISSION_DENIED;
    }

    int ss_id = get_ss_for_file(request->filename);
    if (ss_id < 0)
        return ERR_FILE_NOT_FOUND;

    // Get SS info
    StorageServerInfo *ss = NULL;
    for (int i = 0; i < ns.ss_count; i++)
    {
        if (ns.storage_servers[i].id == ss_id)
        {
            ss = &ns.storage_servers[i];
            break;
        }
    }

    if (!ss)
        return ERR_SERVER_UNAVAILABLE;

    // Forward to SS
    int ss_socket = connect_to_server(ss->ip, ss->client_port);
    if (ss_socket < 0)
        return ERR_SERVER_UNAVAILABLE;

    send_packet(ss_socket, request);

    receive_packet(ss_socket, response);
    close(ss_socket);

    return response->error_code;
}

// Handle EXEC operation
int handle_exec(Packet *request, Packet *response)
{
    if (!check_access(request->filename, request->username, ACCESS_READ))
    {
        return ERR_PERMISSION_DENIED;
    }

    int ss_id = get_ss_for_file(request->filename);
    if (ss_id < 0)
        return ERR_FILE_NOT_FOUND;

    // Get file content from SS
    StorageServerInfo *ss = NULL;
    for (int i = 0; i < ns.ss_count; i++)
    {
        if (ns.storage_servers[i].id == ss_id)
        {
            ss = &ns.storage_servers[i];
            break;
        }
    }

    if (!ss)
        return ERR_SERVER_UNAVAILABLE;

    int ss_socket = connect_to_server(ss->ip, ss->client_port);
    if (ss_socket < 0)
        return ERR_SERVER_UNAVAILABLE;

    Packet read_req = *request;
    read_req.operation = OP_READ;
    send_packet(ss_socket, &read_req);

    Packet read_resp;
    receive_packet(ss_socket, &read_resp);
    close(ss_socket);

    if (read_resp.error_code != SUCCESS)
    {
        return read_resp.error_code;
    }

    // Execute commands
    FILE *fp = popen(read_resp.data, "r");
    if (!fp)
    {
        return ERR_INVALID_OPERATION;
    }

    char output[MAX_BUFFER_SIZE] = "";
    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        strcat(output, line);
    }
    pclose(fp);

    strncpy(response->data, output, MAX_BUFFER_SIZE - 1);
    response->data_len = strlen(output);

    return SUCCESS;
}

// BONUS FEATURE: Handle CREATE_FOLDER operation
int handle_create_folder(Packet *request, Packet *response)
{
    pthread_mutex_lock(&ns.folder_lock);

    // Normalize path - remove leading '/' if present
    char normalized_path[MAX_PATH];
    const char *path_to_use = request->folder_path;
    if (path_to_use[0] == '/')
    {
        path_to_use++; // Skip leading '/'
    }
    strncpy(normalized_path, path_to_use, MAX_PATH - 1);
    normalized_path[MAX_PATH - 1] = '\0';

    // Check if folder already exists
    for (int i = 0; i < ns.folder_count; i++)
    {
        if (strcmp(ns.folders[i].path, normalized_path) == 0)
        {
            pthread_mutex_unlock(&ns.folder_lock);
            return ERR_FOLDER_EXISTS;
        }
    }

    // Create new folder entry
    if (ns.folder_count >= MAX_FILES_PER_SS * MAX_STORAGE_SERVERS)
    {
        pthread_mutex_unlock(&ns.folder_lock);
        return ERR_MEMORY_ERROR;
    }

    Folder *folder = &ns.folders[ns.folder_count++];
    
    // Extract just the folder name (last component of path)
    const char *folder_name = strrchr(normalized_path, '/');
    if (folder_name)
    {
        folder_name++; // Skip the '/'
    }
    else
    {
        folder_name = normalized_path; // No slash, use whole path
    }
    
    strncpy(folder->folder_name, folder_name, MAX_FILENAME - 1);
    strncpy(folder->path, normalized_path, MAX_PATH - 1);
    strncpy(folder->owner, request->username, MAX_USERNAME - 1);
    folder->created = time(NULL);

    pthread_mutex_unlock(&ns.folder_lock);

    log_message("name_server", "INFO", "Folder created: %s by %s", 
                normalized_path, request->username);

    return SUCCESS;
}

// BONUS FEATURE: Handle MOVE_FILE operation
int handle_move_file(Packet *request, Packet *response)
{
    pthread_mutex_lock(&ns.file_lock);

    int file_idx = find_file_index_locked(request->filename);
    if (file_idx < 0)
    {
        pthread_mutex_unlock(&ns.file_lock);
        return ERR_FILE_NOT_FOUND;
    }

    FileInfo *file = &ns.files[file_idx];

    // Check if user has access to this file (owner or in ACL)
    int has_access = 0;
    
    if (strcmp(file->metadata.owner, request->username) == 0)
    {
        has_access = 1;
    }
    else
    {
        // Check ACL for any access
        AccessEntry *entry = file->acl;
        while (entry)
        {
            if (strcmp(entry->username, request->username) == 0 && 
                entry->permissions >= ACCESS_READ)
            {
                has_access = 1;
                break;
            }
            entry = entry->next;
        }
    }
    
    // If user doesn't have access, pretend file doesn't exist
    if (!has_access)
    {
        pthread_mutex_unlock(&ns.file_lock);
        return ERR_FILE_NOT_FOUND;
    }

    // Only the owner can move the file
    if (strcmp(file->metadata.owner, request->username) != 0)
    {
        pthread_mutex_unlock(&ns.file_lock);
        return ERR_FILE_NOT_FOUND;  // Hide that file exists if not owner
    }

    // Normalize path - remove leading '/' if present
    char normalized_path[MAX_PATH];
    const char *path_to_use = request->folder_path;
    if (path_to_use[0] == '/')
    {
        path_to_use++; // Skip leading '/'
    }
    strncpy(normalized_path, path_to_use, MAX_PATH - 1);
    normalized_path[MAX_PATH - 1] = '\0';

    // Update folder path for THIS USER only
    set_user_folder_path(file, request->username, normalized_path);
    file->metadata.last_modified = time(NULL);

    pthread_mutex_unlock(&ns.file_lock);

    log_message("name_server", "INFO", "File %s moved to %s by %s (per-user organization)",
                request->filename, normalized_path, request->username);

    return SUCCESS;
}

// BONUS FEATURE: Handle VIEW_FOLDER operation
int handle_view_folder(Packet *request, Packet *response)
{
    pthread_mutex_lock(&ns.file_lock);
    pthread_mutex_lock(&ns.folder_lock);

    // Normalize path - remove leading '/' if present
    char normalized_path[MAX_PATH];
    const char *path_to_use = request->folder_path;
    if (path_to_use[0] == '/')
    {
        path_to_use++; // Skip leading '/'
    }
    strncpy(normalized_path, path_to_use, MAX_PATH - 1);
    normalized_path[MAX_PATH - 1] = '\0';

    // If path is empty or just whitespace, treat as root
    if (normalized_path[0] == '\0' || strcmp(normalized_path, " ") == 0)
    {
        strcpy(normalized_path, "");
    }
    
    // Check if the folder exists (except for root)
    if (normalized_path[0] != '\0')
    {
        int folder_exists = 0;
        for (int i = 0; i < ns.folder_count; i++)
        {
            if (strcmp(ns.folders[i].path, normalized_path) == 0)
            {
                folder_exists = 1;
                break;
            }
        }
        
        if (!folder_exists)
        {
            pthread_mutex_unlock(&ns.folder_lock);
            pthread_mutex_unlock(&ns.file_lock);
            return ERR_FOLDER_NOT_FOUND;
        }
    }

    char output[MAX_BUFFER_SIZE] = "";
    char line[256];

    // List all folders in the path
    snprintf(line, sizeof(line), "Folders in %s:\n", 
             normalized_path[0] == '\0' ? "root" : normalized_path);
    strcat(output, line);

    int folder_found = 0;
    for (int i = 0; i < ns.folder_count; i++)
    {
        // Only show folders owned by the user
        if (strcmp(ns.folders[i].owner, request->username) != 0)
        {
            continue;
        }
        
        // For root, show folders that don't contain '/'
        if (normalized_path[0] == '\0')
        {
            // Root level - show folders without '/' in path
            if (strchr(ns.folders[i].path, '/') == NULL && ns.folders[i].path[0] != '\0')
            {
                snprintf(line, sizeof(line), "  [DIR] %s (Owner: %s)\n",
                         ns.folders[i].folder_name, ns.folders[i].owner);
                strcat(output, line);
                folder_found = 1;
            }
        }
        else
        {
            // Check if folder is directly under the requested path
            // Should match "path/subfolder" but not "path/sub/deep"
            int path_len = strlen(normalized_path);
            if (strncmp(ns.folders[i].path, normalized_path, path_len) == 0 &&
                ns.folders[i].path[path_len] == '/')
            {
                // Check if there are more slashes after this (indicating deeper nesting)
                const char *rest = ns.folders[i].path + path_len + 1;
                if (strchr(rest, '/') == NULL)
                {
                    snprintf(line, sizeof(line), "  [DIR] %s (Owner: %s)\n",
                             ns.folders[i].folder_name, ns.folders[i].owner);
                    strcat(output, line);
                    folder_found = 1;
                }
            }
        }
    }

    if (!folder_found)
    {
        strcat(output, "  (no folders)\n");
    }

    // List all files in the path
    snprintf(line, sizeof(line), "\nFiles in %s:\n", 
             normalized_path[0] == '\0' ? "root" : normalized_path);
    strcat(output, line);

    int file_found = 0;
    for (int i = 0; i < ns.file_count; i++)
    {
        // Check if user has access to this file (owner or in ACL)
        int has_access = 0;
        
        // Owner has access
        if (strcmp(ns.files[i].metadata.owner, request->username) == 0)
        {
            has_access = 1;
        }
        else
        {
            // Check ACL for read access
            AccessEntry *entry = ns.files[i].acl;
            while (entry)
            {
                if (strcmp(entry->username, request->username) == 0 && 
                    entry->permissions >= ACCESS_READ)
                {
                    has_access = 1;
                    break;
                }
                entry = entry->next;
            }
        }
        
        if (!has_access)
        {
            continue; // Skip files user doesn't have access to
        }
        
        // Get THIS USER's folder path for the file
        const char *user_folder = get_user_folder_path(&ns.files[i], request->username);
        
        // Check if file is in the requested folder for this user
        if (strcmp(user_folder, normalized_path) == 0)
        {
            snprintf(line, sizeof(line), "  [FILE] %s (Owner: %s, Size: %d chars)\n",
                     ns.files[i].metadata.filename, ns.files[i].metadata.owner,
                     ns.files[i].metadata.char_count);
            strcat(output, line);
            file_found = 1;
        }
    }

    if (!file_found)
    {
        strcat(output, "  (no files)\n");
    }

    pthread_mutex_unlock(&ns.folder_lock);
    pthread_mutex_unlock(&ns.file_lock);

    strncpy(response->data, output, MAX_BUFFER_SIZE - 1);
    response->data_len = strlen(output);

    return SUCCESS;
}

// BONUS FEATURE: Handle REQUEST_ACCESS operation
int handle_request_access(Packet *request, Packet *response)
{
    // Check if file exists
    pthread_mutex_lock(&ns.file_lock);
    int file_idx = find_file_index_locked(request->filename);
    pthread_mutex_unlock(&ns.file_lock);
    
    if (file_idx < 0)
    {
        return ERR_FILE_NOT_FOUND;
    }

    pthread_mutex_lock(&ns.request_lock);

    // Check if request already exists
    AccessRequest *curr = ns.access_requests;
    while (curr)
    {
        if (strcmp(curr->filename, request->filename) == 0 &&
            strcmp(curr->requester, request->username) == 0)
        {
            pthread_mutex_unlock(&ns.request_lock);
            return ERR_REQUEST_EXISTS;
        }
        curr = curr->next;
    }

    // Create new access request
    AccessRequest *new_req = (AccessRequest *)malloc(sizeof(AccessRequest));
    if (!new_req)
    {
        pthread_mutex_unlock(&ns.request_lock);
        return ERR_MEMORY_ERROR;
    }

    strncpy(new_req->filename, request->filename, MAX_FILENAME - 1);
    strncpy(new_req->requester, request->username, MAX_USERNAME - 1);
    new_req->requested_permission = request->flags; // READ or WRITE
    new_req->requested_at = time(NULL);
    new_req->next = ns.access_requests;
    ns.access_requests = new_req;

    pthread_mutex_unlock(&ns.request_lock);

    log_message("name_server", "INFO", "Access request created: %s requests %s access to %s",
                request->username, (request->flags == ACCESS_READ ? "READ" : "WRITE"),
                request->filename);

    return SUCCESS;
}

// BONUS FEATURE: Handle LIST_REQUESTS operation
int handle_list_requests(Packet *request, Packet *response)
{
    pthread_mutex_lock(&ns.request_lock);
    pthread_mutex_lock(&ns.file_lock);

    char output[MAX_BUFFER_SIZE] = "";
    char line[256];

    snprintf(line, sizeof(line), "Access Requests for your files:\n");
    strcat(output, line);

    AccessRequest *curr = ns.access_requests;
    int count = 0;

    while (curr)
    {
        // Check if current user owns this file
        int file_idx = find_file_index_locked(curr->filename);
        if (file_idx >= 0)
        {
            FileInfo *file = &ns.files[file_idx];
            if (strcmp(file->metadata.owner, request->username) == 0)
            {
                snprintf(line, sizeof(line), "  %d. %s requests %s access to %s\n",
                         ++count, curr->requester,
                         (curr->requested_permission == ACCESS_READ ? "READ" : "WRITE"),
                         curr->filename);
                strcat(output, line);
            }
        }
        curr = curr->next;
    }

    if (count == 0)
    {
        strcat(output, "  No pending access requests.\n");
    }

    pthread_mutex_unlock(&ns.file_lock);
    pthread_mutex_unlock(&ns.request_lock);

    strncpy(response->data, output, MAX_BUFFER_SIZE - 1);
    response->data_len = strlen(output);

    return SUCCESS;
}

// BONUS FEATURE: Handle APPROVE_REQUEST operation
int handle_approve_request(Packet *request, Packet *response)
{
    pthread_mutex_lock(&ns.request_lock);
    pthread_mutex_lock(&ns.file_lock);

    // Find the request
    AccessRequest *prev = NULL;
    AccessRequest *curr = ns.access_requests;
    int found = 0;

    while (curr)
    {
        if (strcmp(curr->filename, request->filename) == 0 &&
            strcmp(curr->requester, request->data) == 0) // data contains requester username
        {
            // Check if current user owns the file
            int file_idx = find_file_index_locked(request->filename);
            if (file_idx < 0)
            {
                pthread_mutex_unlock(&ns.file_lock);
                pthread_mutex_unlock(&ns.request_lock);
                return ERR_FILE_NOT_FOUND;
            }

            FileInfo *file = &ns.files[file_idx];
            if (strcmp(file->metadata.owner, request->username) != 0)
            {
                pthread_mutex_unlock(&ns.file_lock);
                pthread_mutex_unlock(&ns.request_lock);
                return ERR_PERMISSION_DENIED;
            }

            // Check if requester user exists
            if (!user_exists(curr->requester))
            {
                // Save requester name before freeing
                char requester_name[MAX_USERNAME];
                strncpy(requester_name, curr->requester, MAX_USERNAME - 1);
                requester_name[MAX_USERNAME - 1] = '\0';
                
                // User no longer exists, remove the request
                if (prev)
                {
                    prev->next = curr->next;
                }
                else
                {
                    ns.access_requests = curr->next;
                }
                free(curr);
                pthread_mutex_unlock(&ns.file_lock);
                pthread_mutex_unlock(&ns.request_lock);
                log_message("name_server", "ERROR", "Requester '%s' not found", requester_name);
                return ERR_USER_NOT_FOUND;
            }

            // Add ACL entry
            AccessEntry *new_entry = (AccessEntry *)malloc(sizeof(AccessEntry));
            if (!new_entry)
            {
                pthread_mutex_unlock(&ns.file_lock);
                pthread_mutex_unlock(&ns.request_lock);
                return ERR_MEMORY_ERROR;
            }

            strncpy(new_entry->username, curr->requester, MAX_USERNAME - 1);
            new_entry->permissions = curr->requested_permission;
            new_entry->next = file->acl;
            file->acl = new_entry;

            // Remove request from list
            if (prev)
            {
                prev->next = curr->next;
            }
            else
            {
                ns.access_requests = curr->next;
            }

            free(curr);
            found = 1;

            log_message("name_server", "INFO", "Access approved: %s granted %s access to %s",
                        new_entry->username,
                        (new_entry->permissions == ACCESS_READ ? "READ" : "WRITE"),
                        request->filename);

            break;
        }
        prev = curr;
        curr = curr->next;
    }

    pthread_mutex_unlock(&ns.file_lock);
    pthread_mutex_unlock(&ns.request_lock);

    return found ? SUCCESS : ERR_FILE_NOT_FOUND;
}

// BONUS FEATURE: Handle DENY_REQUEST operation
int handle_deny_request(Packet *request, Packet *response)
{
    pthread_mutex_lock(&ns.request_lock);
    pthread_mutex_lock(&ns.file_lock);

    // Find and remove the request
    AccessRequest *prev = NULL;
    AccessRequest *curr = ns.access_requests;
    int found = 0;

    while (curr)
    {
        if (strcmp(curr->filename, request->filename) == 0 &&
            strcmp(curr->requester, request->data) == 0) // data contains requester username
        {
            // Check if current user owns the file
            int file_idx = find_file_index_locked(request->filename);
            if (file_idx < 0)
            {
                pthread_mutex_unlock(&ns.file_lock);
                pthread_mutex_unlock(&ns.request_lock);
                return ERR_FILE_NOT_FOUND;
            }

            FileInfo *file = &ns.files[file_idx];
            if (strcmp(file->metadata.owner, request->username) != 0)
            {
                pthread_mutex_unlock(&ns.file_lock);
                pthread_mutex_unlock(&ns.request_lock);
                return ERR_PERMISSION_DENIED;
            }

            // Remove request from list
            if (prev)
            {
                prev->next = curr->next;
            }
            else
            {
                ns.access_requests = curr->next;
            }

            log_message("name_server", "INFO", "Access denied: %s's request for %s was denied by %s",
                        curr->requester, request->filename, request->username);

            free(curr);
            found = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    pthread_mutex_unlock(&ns.file_lock);
    pthread_mutex_unlock(&ns.request_lock);

    return found ? SUCCESS : ERR_FILE_NOT_FOUND;
}

// Handle UPDATE_METADATA operation
int handle_update_metadata(Packet *request, Packet *response)
{
    pthread_mutex_lock(&ns.file_lock);

    int file_idx = find_file_index_locked(request->filename);
    if (file_idx < 0)
    {
        pthread_mutex_unlock(&ns.file_lock);
        log_message("name_server", "WARNING", "Metadata update for unknown file: %s", request->filename);
        return ERR_FILE_NOT_FOUND;
    }

    FileInfo *file = &ns.files[file_idx];

    // Update word count and character count
    file->metadata.word_count = request->word_index;   // Reused field
    file->metadata.char_count = request->sentence_num; // Reused field
    file->metadata.last_modified = time(NULL);

    pthread_mutex_unlock(&ns.file_lock);

    log_message("name_server", "INFO", "Metadata updated for %s: words=%d, chars=%d",
                request->filename, file->metadata.word_count, file->metadata.char_count);

    // Invalidate cache to ensure fresh data on next access
    cache_invalidate(request->filename);

    return SUCCESS;
}

