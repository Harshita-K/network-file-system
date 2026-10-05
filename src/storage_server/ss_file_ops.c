#include "../../include/ss_state.h"
#include "../../include/ss_file_ops.h"
#include "../../include/ss_metadata.h"
#include "../../include/ss_cache.h"
#include "../../include/file_structure.h"

void push_undo(const char *filename, FileStructure *file)
{
    int idx = get_file_index_for_undo(filename);
    if (idx < 0)
        return;

    // Clone file structure
    FileStructure *clone = clone_file_structure(file);
    if (!clone)
        return;

    // Push to stack
    int top = ss.undo_top[idx];
    if (top >= MAX_UNDO_STACK - 1)
    {
        // Stack full, remove oldest
        free_file_structure(ss.undo_stack[idx][0]);
        for (int i = 0; i < MAX_UNDO_STACK - 1; i++)
        {
            ss.undo_stack[idx][i] = ss.undo_stack[idx][i + 1];
        }
        ss.undo_stack[idx][MAX_UNDO_STACK - 1] = clone;
    }
    else
    {
        ss.undo_stack[idx][++top] = clone;
        ss.undo_top[idx] = top;
    }
}

// Pop from undo stack
FileStructure *pop_undo(const char *filename)
{
    int idx = get_file_index_for_undo(filename);
    if (idx < 0 || ss.undo_top[idx] < 0)
        return NULL;

    FileStructure *file = ss.undo_stack[idx][ss.undo_top[idx]];
    ss.undo_top[idx]--;

    return file;
}

// Create file
int handle_create_file(const char *filename, const char *owner)
{
    char filepath[MAX_PATH];
    get_file_path(filename, filepath);

    // Check if file exists
    if (access(filepath, F_OK) == 0)
    {
        return ERR_FILE_EXISTS;
    }

    // Get file index for locking
    int file_idx = get_file_index_for_undo(filename);
    if (file_idx >= 0)
    {
        pthread_rwlock_wrlock(&ss.file_locks[file_idx]); // Write lock for creation
    }

    // Create empty file
    FileStructure *file = create_file_structure();
    if (!file)
    {
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_MEMORY_ERROR;
    }

    int result = save_file_to_disk(filename, file);
    free_file_structure(file);

    if (result == SUCCESS) {
        // Initialize file version in metadata.db
        save_metadata_db(filename, 1, owner, time(NULL), 0);
    }

    if (file_idx >= 0)
    {
        pthread_rwlock_unlock(&ss.file_locks[file_idx]);
    }

    log_message("storage_server", "INFO", "File created: %s by %s", filename, owner);
    return result;
}

// Delete file
int handle_delete_file(const char *filename)
{
    // Get file index for locking
    int file_idx = get_file_index_for_undo(filename);
    if (file_idx >= 0)
    {
        pthread_rwlock_wrlock(&ss.file_locks[file_idx]); // Write lock for deletion
    }

    char filepath[MAX_PATH];
    get_file_path(filename, filepath);

    if (remove(filepath) != 0)
    {
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_FILE_NOT_FOUND;
    }
    
    // Also delete undo files
    for (int i = 0; i < MAX_UNDO_STACK; i++)
    {
        char undo_path[MAX_PATH];
        snprintf(undo_path, sizeof(undo_path), "%s/.undo_%s_%d", ss.base_path, filename, i);
        remove(undo_path);  // Ignore errors
    }
    
    // Delete checkpoint files
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s/.checkpoint_%s_*", ss.base_path, filename);
    char command[MAX_PATH + 50];
    snprintf(command, sizeof(command), "rm -f %s 2>/dev/null", pattern);
    system(command);

    log_message("storage_server", "INFO", "File deleted: %s", filename);

    if (file_idx >= 0)
    {
        pthread_rwlock_unlock(&ss.file_locks[file_idx]);
    }
    
    // Notify name server for replication (delete on replica)
    Packet notify = {0};
    notify.operation = OP_DELETE;
    notify.ss_id = ss.id;
    strncpy(notify.filename, filename, MAX_FILENAME - 1);
    
    if (send_packet(ss.nm_socket, &notify) < 0)
    {
        log_message("storage_server", "WARNING", "Failed to notify name server for delete replication of %s", filename);
    }

    return SUCCESS;
}

// Read file
int handle_read_file(const char *filename, char *content)
{
    // Get file index for locking
    int file_idx = get_file_index_for_undo(filename);
    if (file_idx >= 0)
    {
        pthread_rwlock_rdlock(&ss.file_locks[file_idx]); // Read lock - allows concurrent reads!
    }

    FileStructure *file = load_file_from_disk(filename);
    if (!file)
    {
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_FILE_NOT_FOUND;
    }

    char *text = file_to_text(file);
    if (!text)
    {
        free_file_structure(file);
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_MEMORY_ERROR;
    }

    strncpy(content, text, MAX_BUFFER_SIZE - 1);
    content[MAX_BUFFER_SIZE - 1] = '\0';

    free(text);
    free_file_structure(file);

    if (file_idx >= 0)
    {
        pthread_rwlock_unlock(&ss.file_locks[file_idx]);
    }

    return SUCCESS;
}

// Helper function: parse text into individual words (including delimiters as separate words)
// but DON'T create sentence boundaries - just return array of words
typedef struct {
    char words[MAX_SENTENCE_LENGTH / 2][MAX_WORD_LENGTH];  // Max possible words
    int count;
} ParsedWords;

ParsedWords parse_into_words(const char* text)
{
    ParsedWords result;
    result.count = 0;
    
    char word_buffer[MAX_WORD_LENGTH];
    int word_index = 0;
    int i = 0;
    
    while (text[i] != '\0' && result.count < MAX_SENTENCE_LENGTH / 2)
    {
        if (text[i] == ' ' || text[i] == '\n' || text[i] == '\t' || text[i] == '\r')
        {
            // Space or whitespace - end of word
            if (word_index > 0)
            {
                word_buffer[word_index] = '\0';
                strncpy(result.words[result.count], word_buffer, MAX_WORD_LENGTH - 1);
                result.words[result.count][MAX_WORD_LENGTH - 1] = '\0';
                result.count++;
                word_index = 0;
            }
        }
        else if (is_sentence_delimiter(text[i]))
        {
            // Sentence delimiter - add current word then delimiter as separate word
            if (word_index > 0)
            {
                word_buffer[word_index] = '\0';
                strncpy(result.words[result.count], word_buffer, MAX_WORD_LENGTH - 1);
                result.words[result.count][MAX_WORD_LENGTH - 1] = '\0';
                result.count++;
                word_index = 0;
            }
            
            // Add delimiter as a separate word
            result.words[result.count][0] = text[i];
            result.words[result.count][1] = '\0';
            result.count++;
        }
        else
        {
            // Regular character - add to word buffer
            if (word_index < MAX_WORD_LENGTH - 1)
            {
                word_buffer[word_index++] = text[i];
            }
        }
        i++;
    }
    
    // Add last word if exists
    if (word_index > 0 && result.count < MAX_SENTENCE_LENGTH / 2)
    {
        word_buffer[word_index] = '\0';
        strncpy(result.words[result.count], word_buffer, MAX_WORD_LENGTH - 1);
        result.words[result.count][MAX_WORD_LENGTH - 1] = '\0';
        result.count++;
    }
    
    return result;
}

// Get or create shared file cache entry

int handle_write_file(const char *filename, int sentence_num, int word_index, const char *content, const char *lock_token)
{
    log_message("storage_server", "INFO", 
               "WRITE request: file=%s, sentence=%d, word_index=%d, lock_token=%s",
               filename, sentence_num, word_index, lock_token);
    
    // Check if there's an existing write session for this lock
    ActiveWriteSession* session = find_write_session(filename, sentence_num, lock_token);
    
    log_message("storage_server", "DEBUG", 
               "Session lookup result: %s", session ? "found existing" : "creating new");
    
    if (!session)
    {
        // First write in this session - get shared file cache
        int cache_idx = get_file_cache(filename);
        if (cache_idx < 0)
        {
            log_message("storage_server", "ERROR", "File not found: %s", filename);
            return ERR_FILE_NOT_FOUND;
        }
        
        log_message("storage_server", "DEBUG", "Got file cache at index %d", cache_idx);
        
        // Create new session
        session = create_write_session(filename, sentence_num, lock_token, cache_idx);
        if (!session)
        {
            release_file_cache(cache_idx);
            return ERR_MEMORY_ERROR;  // Too many concurrent writes
        }
        
        SharedFileCache* cache = &ss.file_cache[cache_idx];
        pthread_mutex_lock(&cache->cache_lock);
        
        FileStructure* file = cache->file_structure;
        
        // Save current state for undo
        push_undo(filename, file);

        // Create missing sentences if needed
        while (file->sentence_count < sentence_num)
        {
            SentenceNode *new_sentence = create_sentence_node();
            if (!new_sentence)
            {
                pthread_mutex_unlock(&cache->cache_lock);
                close_write_session(session);
                return ERR_MEMORY_ERROR;
            }
            // Add default full stop delimiter to empty sentences
            add_word_to_sentence(new_sentence, ".");
            add_sentence_to_file(file, new_sentence);
        }

        // Traverse to the specific sentence node and lock it
        session->locked_sentence = get_sentence(file, sentence_num);
        if (!session->locked_sentence)
        {
            pthread_mutex_unlock(&cache->cache_lock);
            close_write_session(session);
            return ERR_INVALID_SENTENCE;
        }

        log_message("storage_server", "DEBUG", 
                   "User attempting to lock sentence %d: ptr=%p, file=%s",
                   sentence_num, (void*)session->locked_sentence, filename);

        // Try to lock the sentence pointer - use trylock to detect if already locked!
        int lock_result = pthread_mutex_trylock(&session->locked_sentence->lock);
        
        log_message("storage_server", "DEBUG", 
                   "Lock attempt result: %d (0=success, EBUSY=%d), ptr=%p",
                   lock_result, EBUSY, (void*)session->locked_sentence);
        
        if (lock_result != 0)
        {
            // Lock is already held by another user
            pthread_mutex_unlock(&cache->cache_lock);
            close_write_session(session);
            
            log_message("storage_server", "WARNING", 
                       "Sentence %d of %s (ptr=%p) is already locked by another user (result=%d)",
                       sentence_num, filename, (void*)session->locked_sentence, lock_result);
            
            return ERR_FILE_LOCKED;
        }
        
        // CREATE WORKING COPY (Copy-on-Write)
        session->working_copy = duplicate_sentence(session->locked_sentence);
        if (!session->working_copy)
        {
            pthread_mutex_unlock(&session->locked_sentence->lock);
            pthread_mutex_unlock(&cache->cache_lock);
            close_write_session(session);
            return ERR_MEMORY_ERROR;
        }
        
        log_message("storage_server", "INFO", 
                   "Successfully locked sentence %d (ptr=%p) in %s, created working copy", 
                   sentence_num, (void*)session->locked_sentence, filename);
        
        pthread_mutex_unlock(&cache->cache_lock);
    }

    // At this point, we have a session with a locked sentence and a working copy
    // Work ONLY on the working copy - NOT the shared cache!

    // At this point, we have a session with a locked sentence and a working copy
    // Work ONLY on the working copy - NOT the shared cache!
    int cache_idx = session->cache_index;
    SharedFileCache* cache = &ss.file_cache[cache_idx];
    
    pthread_mutex_lock(&cache->cache_lock);
    
    // Work on the WORKING COPY, not the original!
    SentenceNode* sentence = session->working_copy;
    FileStructure* file = cache->file_structure;

    // Check if this is a placeholder sentence (only contains ".")
    int is_placeholder = (sentence->word_count == 1 &&
                          sentence->words &&
                          strcmp(sentence->words->word, ".") == 0);

    // For pointer-based locking: keep ALL words in the SAME sentence node
    // until ETIRW is called (which will trigger sentence splitting if needed)
    
    // Parse content into individual words (including delimiters as separate words)
    // This is KEY: we parse by words with delimiters separated, but DON'T create sentence boundaries!
    ParsedWords parsed = parse_into_words(content);
    
    // Remove trailing delimiter from sentence temporarily if it exists
    int has_delimiter = 0;
    char saved_delimiter[2] = {0};
    if (sentence->word_count > 0)
    {
        WordNode *last_word = sentence->words;
        while (last_word->next)
        {
            last_word = last_word->next;
        }
        if (is_sentence_delimiter(last_word->word[0]) && strlen(last_word->word) == 1)
        {
            has_delimiter = 1;
            saved_delimiter[0] = last_word->word[0];
            delete_word(sentence, sentence->word_count - 1);
        }
    }

    // If placeholder sentence, remove it but DON'T preserve the delimiter
    // because it's just a placeholder, not real content
    if (is_placeholder)
    {
        delete_word(sentence, 0);
        has_delimiter = 0;  // Don't preserve placeholder delimiter
        saved_delimiter[0] = '\0';
    }

    // Insert all parsed words at the specified position
    int insert_pos = word_index;
    if (insert_pos > sentence->word_count)
    {
        insert_pos = sentence->word_count;
    }
    
    for (int i = 0; i < parsed.count; i++)
    {
        if (insert_pos >= sentence->word_count)
        {
            // Append to end
            add_word_to_sentence(sentence, parsed.words[i]);
            insert_pos++;
        }
        else
        {
            // Insert at position
            insert_word(sentence, insert_pos, parsed.words[i]);
            insert_pos++;
        }
    }

    // Add delimiter back at the end if it was present originally
    if (has_delimiter)
    {
        add_word_to_sentence(sentence, saved_delimiter);
    }

    // DON'T update file stats here - we're only modifying the working copy!
    // File stats will be updated when we commit on ETIRW in close_write_session()

    // DON'T save file to disk here - only save on ETIRW!
    // This keeps the session active and lock held
    int result = SUCCESS;

    // Send lock refresh to name server to prevent stale lock timeout
    Packet update_pkt = {0};
    update_pkt.operation = OP_ACQUIRE_LOCK;  // Refresh lock timestamp
    update_pkt.ss_id = ss.id;
    strncpy(update_pkt.filename, filename, MAX_FILENAME - 1);
    update_pkt.sentence_num = sentence_num;
    strncpy(update_pkt.lock_token, lock_token, 63);
    
    if (ss.nm_socket >= 0)
    {
        send_packet(ss.nm_socket, &update_pkt);
    }

    // Note: We do NOT unlock the sentence or close the session here
    // The session stays active until ETIRW (handled elsewhere)
    
    pthread_mutex_unlock(&cache->cache_lock);

    return result;
}

// BONUS FEATURE: Save checkpoint

int handle_undo(const char *filename)
{
    // Get file index for locking
    int file_idx = get_file_index_for_undo(filename);
    if (file_idx >= 0)
    {
        pthread_rwlock_wrlock(&ss.file_locks[file_idx]); // Write lock for undo
    }

    FileStructure *old_file = pop_undo(filename);
    if (!old_file)
    {
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_INVALID_OPERATION;
    }

    int result = save_file_to_disk(filename, old_file);
    
    // Increment version number after undo
    if (result == SUCCESS)
    {
        increment_file_version(filename, "system");
    }
    
    // Calculate metadata before freeing the file
    int word_count = old_file->total_words;
    int char_count = old_file->total_chars;
    
    free_file_structure(old_file);

    if (file_idx >= 0)
    {
        pthread_rwlock_unlock(&ss.file_locks[file_idx]);
    }

    if (result == SUCCESS)
    {
        log_message("storage_server", "INFO", "Undo performed on: %s", filename);
        
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
                log_message("storage_server", "INFO", "Metadata update sent for %s after undo (words=%d, chars=%d)", 
                           filename, word_count, char_count);
            }
            else
            {
                log_message("storage_server", "WARNING", "Failed to send metadata update for %s after undo", filename);
            }
        }
        
        // Notify name server for replication
        Packet notify = {0};
        notify.operation = OP_REPLICA_WRITE;
        notify.ss_id = ss.id;
        strncpy(notify.filename, filename, MAX_FILENAME - 1);
        
        if (send_packet(ss.nm_socket, &notify) < 0)
        {
            log_message("storage_server", "WARNING", "Failed to notify name server for undo replication of %s", filename);
        }
    }
    
    return result;
}

// Get file info
int handle_file_info(const char *filename, Packet *response)
{
    // Get file index for locking
    int file_idx = get_file_index_for_undo(filename);
    if (file_idx >= 0)
    {
        pthread_rwlock_rdlock(&ss.file_locks[file_idx]); // Read lock - concurrent reads OK
    }

    char filepath[MAX_PATH];
    get_file_path(filename, filepath);

    struct stat st;
    if (stat(filepath, &st) != 0)
    {
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_FILE_NOT_FOUND;
    }

    FileStructure *file = load_file_from_disk(filename);
    if (!file)
    {
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        return ERR_FILE_NOT_FOUND;
    }

    char *owner = get_file_owner(filename);
    snprintf(response->data, MAX_BUFFER_SIZE,
             "File: %s\nOwner: %s\nSize: %ld bytes\nWords: %d\nCharacters: %d\nSentences: %d\nLast Modified: %s",
             filename, owner, st.st_size, file->total_words, file->total_chars,
             file->sentence_count, format_time(st.st_mtime));
    free(owner);

    free_file_structure(file);

    if (file_idx >= 0)
    {
        pthread_rwlock_unlock(&ss.file_locks[file_idx]);
    }

    return SUCCESS;
}

// Stream file content
void handle_stream_file(int client_socket, const char *filename)
{
    // Get file index for locking
    int file_idx = get_file_index_for_undo(filename);
    if (file_idx >= 0)
    {
        pthread_rwlock_rdlock(&ss.file_locks[file_idx]); // Read lock - concurrent reads OK
    }

    FileStructure *file = load_file_from_disk(filename);
    if (!file)
    {
        if (file_idx >= 0)
            pthread_rwlock_unlock(&ss.file_locks[file_idx]);
        Packet error_pkt = {0};
        error_pkt.operation = OP_ERROR;
        error_pkt.error_code = ERR_FILE_NOT_FOUND;
        send_packet(client_socket, &error_pkt);
        return;
    }

    // Stream word by word with 0.1 second delay
    SentenceNode *sent = file->sentences;
    while (sent)
    {
        WordNode *word = sent->words;
        while (word)
        {
            Packet pkt = {0};
            pkt.operation = OP_STREAM;
            pkt.error_code = SUCCESS;
            strncpy(pkt.data, word->word, MAX_BUFFER_SIZE - 1);
            pkt.data_len = strlen(word->word);

            if (send_packet(client_socket, &pkt) < 0)
            {
                log_message("storage_server", "ERROR", "Failed to stream word");
                free_file_structure(file);
                if (file_idx >= 0)
                    pthread_rwlock_unlock(&ss.file_locks[file_idx]);
                return;
            }

            usleep(100000); // 0.1 second delay
            word = word->next;
        }
        sent = sent->next;
    }

    // Send STOP packet
    Packet stop_pkt = {0};
    stop_pkt.operation = OP_ACK;
    strcpy(stop_pkt.data, "STOP");
    send_packet(client_socket, &stop_pkt);

    free_file_structure(file);

    if (file_idx >= 0)
    {
        pthread_rwlock_unlock(&ss.file_locks[file_idx]);
    }

    log_message("storage_server", "INFO", "File streamed: %s", filename);
}

// Handle client connection
