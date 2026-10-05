#include "../../include/ss_state.h"
#include "../../include/ss_checkpoints.h"
#include "../../include/ss_metadata.h"
#include "../../include/ss_file_ops.h"
#include "../../include/file_structure.h"

int handle_save_checkpoint(const char *filename, const char *tag)
{
    int file_idx = get_file_index_for_undo(filename);
    if (file_idx >= 0)
    {
        pthread_rwlock_rdlock(&ss.file_locks[file_idx]); // Read lock for saving checkpoint
    }

    // Load current file content
    FileStructure *file = load_file_from_disk(filename);
    if (!file)
    {
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_FILE_NOT_FOUND;
    }

    // Save checkpoint to separate file
    char checkpoint_path[MAX_PATH];
    snprintf(checkpoint_path, sizeof(checkpoint_path), "%s/.checkpoint_%s_%s", 
             ss.base_path, filename, tag);

    char *text = file_to_text(file);
    if (!text)
    {
        free_file_structure(file);
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_MEMORY_ERROR;
    }

    FILE *fp = fopen(checkpoint_path, "w");
    if (!fp)
    {
        free(text);
        free_file_structure(file);
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_INVALID_OPERATION;
    }

    fprintf(fp, "%s", text);
    fclose(fp);
    free(text);
    
    // Increment version number after checkpoint
    increment_file_version(filename, "system");
    
    free_file_structure(file);

    if (file_idx >= 0)
    {
        pthread_rwlock_unlock(&ss.file_locks[file_idx]);
    }

    log_message("storage_server", "INFO", "Checkpoint saved: %s (tag: %s)", filename, tag);
    
    // Notify name server for checkpoint replication
    Packet notify = {0};
    notify.operation = OP_REPLICA_CHECKPOINT;
    notify.ss_id = ss.id;
    strncpy(notify.filename, filename, MAX_FILENAME - 1);
    strncpy(notify.checkpoint_tag, tag, MAX_TAG_LENGTH - 1);
    
    if (send_packet(ss.nm_socket, &notify) < 0)
    {
        log_message("storage_server", "WARNING", "Failed to notify name server for checkpoint replication of %s:%s", filename, tag);
    }
    
    return SUCCESS;
}

// BONUS FEATURE: View checkpoint
int handle_view_checkpoint(const char *filename, const char *tag, char *content)
{
    char checkpoint_path[MAX_PATH];
    snprintf(checkpoint_path, sizeof(checkpoint_path), "%s/.checkpoint_%s_%s", 
             ss.base_path, filename, tag);

    FILE *fp = fopen(checkpoint_path, "r");
    if (!fp)
    {
        return ERR_CHECKPOINT_NOT_FOUND;
    }

    // Read checkpoint content
    size_t bytes_read = fread(content, 1, MAX_BUFFER_SIZE - 1, fp);
    content[bytes_read] = '\0';
    fclose(fp);

    log_message("storage_server", "INFO", "Checkpoint viewed: %s (tag: %s)", filename, tag);
    return SUCCESS;
}

// BONUS FEATURE: Revert to checkpoint
int handle_revert_checkpoint(const char *filename, const char *tag)
{
    int file_idx = get_file_index_for_undo(filename);
    if (file_idx >= 0)
    {
        pthread_rwlock_wrlock(&ss.file_locks[file_idx]); // Write lock for reverting
    }

    // Save current version to undo stack before reverting
    FileStructure *current = load_file_from_disk(filename);
    if (current)
    {
        push_undo(filename, current);
    }

    char checkpoint_path[MAX_PATH];
    snprintf(checkpoint_path, sizeof(checkpoint_path), "%s/.checkpoint_%s_%s", 
             ss.base_path, filename, tag);

    FILE *fp = fopen(checkpoint_path, "r");
    if (!fp)
    {
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_CHECKPOINT_NOT_FOUND;
    }

    // Read checkpoint content
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *text = (char *)malloc(size + 1);
    if (!text)
    {
        fclose(fp);
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_MEMORY_ERROR;
    }

    fread(text, 1, size, fp);
    text[size] = '\0';
    fclose(fp);

    // Parse text into file structure
    FileStructure *restored_file = parse_text_to_file(text);
    free(text);

    if (!restored_file)
    {
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_MEMORY_ERROR;
    }

    // Save restored version to disk
    int result = save_file_to_disk(filename, restored_file);
    
    // Increment version number after revert
    if (result == SUCCESS)
    {
        increment_file_version(filename, "system");
    }
    
    // Calculate metadata before freeing the file
    int word_count = restored_file->total_words;
    int char_count = restored_file->total_chars;
    
    free_file_structure(restored_file);

    if (file_idx >= 0)
    {
        pthread_rwlock_unlock(&ss.file_locks[file_idx]);
    }

    if (result == SUCCESS)
    {
        log_message("storage_server", "INFO", "Reverted to checkpoint: %s (tag: %s)", filename, tag);
        
        // Send metadata update to name server
        if (ss.nm_socket >= 0)
        {
            Packet metadata_pkt = {0};
            metadata_pkt.operation = OP_UPDATE_METADATA;
            metadata_pkt.ss_id = ss.id;
            strncpy(metadata_pkt.filename, filename, MAX_FILENAME - 1);
            metadata_pkt.word_index = word_count;
            metadata_pkt.sentence_num = char_count;
            
            if (send_packet(ss.nm_socket, &metadata_pkt) >= 0)
            {
                log_message("storage_server", "INFO", "Metadata update sent for %s after checkpoint revert (words=%d, chars=%d)", 
                           filename, word_count, char_count);
            }
            else
            {
                log_message("storage_server", "WARNING", "Failed to send metadata update for %s after checkpoint revert", filename);
            }
        }
        
        // Notify name server for replication
        Packet notify = {0};
        notify.operation = OP_REPLICA_WRITE;
        notify.ss_id = ss.id;
        strncpy(notify.filename, filename, MAX_FILENAME - 1);
        
        if (send_packet(ss.nm_socket, &notify) < 0)
        {
            log_message("storage_server", "WARNING", "Failed to notify name server for checkpoint revert replication of %s", filename);
        }
    }
    
    return result;
}

// BONUS FEATURE: List checkpoints
int handle_list_checkpoints(const char *filename, char *output)
{
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s/.checkpoint_%s_*", ss.base_path, filename);

    char command[MAX_PATH + 50];
    snprintf(command, sizeof(command), "ls -1 %s 2>/dev/null", pattern);

    FILE *fp = popen(command, "r");
    if (!fp)
    {
        strcpy(output, "No checkpoints found.\n");
        return SUCCESS;
    }

    char line[256];
    strcpy(output, "Checkpoints:\n");
    int count = 0;

    while (fgets(line, sizeof(line), fp))
    {
        // Extract tag from filename
        char *tag_start = strrchr(line, '_');
        if (tag_start)
        {
            tag_start++; // Skip the underscore
            // Remove newline
            tag_start[strcspn(tag_start, "\n")] = 0;
            
            char entry[128];
            snprintf(entry, sizeof(entry), "  - %s\n", tag_start);
            strcat(output, entry);
            count++;
        }
    }
    pclose(fp);

    if (count == 0)
    {
        strcpy(output, "No checkpoints found.\n");
    }

    log_message("storage_server", "INFO", "Listed checkpoints for: %s (%d found)", filename, count);
    return SUCCESS;
}

// Undo last change
