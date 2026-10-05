#include "../../include/ss_state.h"
#include "../../include/ss_cache.h"
#include "../../include/ss_metadata.h"
#include "../../include/file_structure.h"

int get_file_cache(const char* filename)
{
    pthread_mutex_lock(&ss.cache_mgmt_lock);
    
    // Check if already cached
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (ss.file_cache[i].valid && strcmp(ss.file_cache[i].filename, filename) == 0)
        {
            pthread_mutex_unlock(&ss.cache_mgmt_lock);
            return i;
        }
    }
    
    // Find empty slot
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (!ss.file_cache[i].valid)
        {
            // Load file from disk
            FileStructure* file = load_file_from_disk(filename);
            if (!file)
            {
                pthread_mutex_unlock(&ss.cache_mgmt_lock);
                return -1;
            }
            
            strncpy(ss.file_cache[i].filename, filename, MAX_FILENAME - 1);
            ss.file_cache[i].file_structure = file;
            ss.file_cache[i].active_sessions = 0;
            ss.file_cache[i].valid = 1;
            
            pthread_mutex_unlock(&ss.cache_mgmt_lock);
            return i;
        }
    }
    
    pthread_mutex_unlock(&ss.cache_mgmt_lock);
    return -1;  // No available slots
}

// Release file cache entry (decrements session count, frees memory if count reaches 0)
void release_file_cache(int cache_idx)
{
    if (cache_idx < 0 || cache_idx >= MAX_OPEN_FILES) return;
    
    pthread_mutex_lock(&ss.cache_mgmt_lock);
    pthread_mutex_lock(&ss.file_cache[cache_idx].cache_lock);
    
    ss.file_cache[cache_idx].active_sessions--;
    
    if (ss.file_cache[cache_idx].active_sessions <= 0)
    {
        // No more active sessions - free memory and notify for replication
        // NOTE: File has already been saved to disk on each ETIRW in close_write_session()
        if (ss.file_cache[cache_idx].file_structure)
        {
            // Notify name server to trigger replication when LAST session closes
            if (ss.nm_socket >= 0)
            {
                Packet notify_pkt = {0};
                notify_pkt.operation = OP_REPLICA_WRITE;
                notify_pkt.ss_id = ss.id;
                strncpy(notify_pkt.filename, ss.file_cache[cache_idx].filename, MAX_FILENAME - 1);
                
                // Send notification to name server (fire and forget)
                send_packet(ss.nm_socket, &notify_pkt);
                
                log_message("storage_server", "INFO", "Replication notification sent for %s (all sessions closed)", 
                           ss.file_cache[cache_idx].filename);
            }
            
            free_file_structure(ss.file_cache[cache_idx].file_structure);
        }
        
        ss.file_cache[cache_idx].file_structure = NULL;
        ss.file_cache[cache_idx].valid = 0;
        ss.file_cache[cache_idx].active_sessions = 0;
        
        log_message("storage_server", "DEBUG", "Shared cache cleared for %s", 
                   ss.file_cache[cache_idx].filename);
    }
    
    pthread_mutex_unlock(&ss.file_cache[cache_idx].cache_lock);
    pthread_mutex_unlock(&ss.cache_mgmt_lock);
}

// Find active write session
ActiveWriteSession* find_write_session(const char* filename, int sentence_num, const char* lock_token)
{
    pthread_mutex_lock(&ss.sessions_lock);
    
    for (int i = 0; i < MAX_ACTIVE_WRITES; i++)
    {
        if (ss.write_sessions[i].active &&
            strcmp(ss.write_sessions[i].filename, filename) == 0 &&
            ss.write_sessions[i].sentence_num == sentence_num &&
            strcmp(ss.write_sessions[i].lock_token, lock_token) == 0)
        {
            pthread_mutex_unlock(&ss.sessions_lock);
            return &ss.write_sessions[i];
        }
    }
    
    pthread_mutex_unlock(&ss.sessions_lock);
    return NULL;
}

// Create new write session
ActiveWriteSession* create_write_session(const char* filename, int sentence_num, const char* lock_token, int cache_idx)
{
    pthread_mutex_lock(&ss.sessions_lock);
    
    // Find empty slot
    for (int i = 0; i < MAX_ACTIVE_WRITES; i++)
    {
        if (!ss.write_sessions[i].active)
        {
            strncpy(ss.write_sessions[i].filename, filename, MAX_FILENAME - 1);
            strncpy(ss.write_sessions[i].lock_token, lock_token, 63);
            ss.write_sessions[i].sentence_num = sentence_num;
            ss.write_sessions[i].active = 1;
            ss.write_sessions[i].locked_sentence = NULL;
            ss.write_sessions[i].cache_index = cache_idx;
            
            // Increment session count for this file
            pthread_mutex_lock(&ss.cache_mgmt_lock);
            ss.file_cache[cache_idx].active_sessions++;
            pthread_mutex_unlock(&ss.cache_mgmt_lock);
            
            pthread_mutex_unlock(&ss.sessions_lock);
            return &ss.write_sessions[i];
        }
    }
    
    pthread_mutex_unlock(&ss.sessions_lock);
    return NULL;  // No available slots
}

// Duplicate a sentence node (Copy-on-Write)
SentenceNode* duplicate_sentence(SentenceNode *original)
{
    if (!original) return NULL;
    
    SentenceNode *copy = create_sentence_node();
    if (!copy) return NULL;
    
    copy->word_count = 0;
    copy->next = NULL;  // Don't link to other sentences - it's a standalone copy
    
    // Duplicate all words in the sentence
    WordNode *current_word = original->words;
    while (current_word)
    {
        add_word_to_sentence(copy, current_word->word);
        current_word = current_word->next;
    }
    
    return copy;
}

// Free a sentence node (without freeing the entire chain)
void free_single_sentence(SentenceNode *sentence)
{
    if (!sentence) return;
    
    // Free all words
    free_word_list(sentence->words);
    
    // Destroy the mutex
    pthread_mutex_destroy(&sentence->lock);
    
    // Free the sentence node itself
    free(sentence);
}

// Release test lock without committing (for name server lock verification)
void release_test_lock(ActiveWriteSession* session)
{
    if (!session) return;
    
    pthread_mutex_lock(&ss.sessions_lock);
    
    if (session->active && session->locked_sentence)
    {
        // Just unlock the sentence, don't commit any changes
        pthread_mutex_unlock(&session->locked_sentence->lock);
        
        log_message("storage_server", "INFO", 
                   "Released test lock on sentence %d (ptr=%p) for %s",
                   session->sentence_num, (void*)session->locked_sentence, session->filename);
        
        // Free the working copy (it was never written to the real file)
        if (session->working_copy)
        {
            free_single_sentence(session->working_copy);
        }
        
        // Release file cache
        if (session->cache_index >= 0)
        {
            release_file_cache(session->cache_index);
        }
    }
    
    // Mark session as inactive
    session->active = 0;
    session->locked_sentence = NULL;
    session->working_copy = NULL;
    session->cache_index = -1;
    
    pthread_mutex_unlock(&ss.sessions_lock);
}

// Close write session (called on ETIRW) - Replace THIS user's locked sentence only
void close_write_session(ActiveWriteSession* session)
{
    if (!session) return;
    
    pthread_mutex_lock(&ss.sessions_lock);
    
    if (session->active && session->cache_index >= 0 && session->locked_sentence && session->working_copy)
    {
        int cache_idx = session->cache_index;
        SharedFileCache* cache = &ss.file_cache[cache_idx];
        
        pthread_mutex_lock(&cache->cache_lock);
        
        SentenceNode* original_sent = session->locked_sentence;
        SentenceNode* working_sent = session->working_copy;
        FileStructure* file = cache->file_structure;
        
        if (file && original_sent && working_sent)
        {
            // Step 1: Parse the working copy to detect sentence boundaries
            char sentence_text[MAX_BUFFER_SIZE] = {0};
            WordNode* w = working_sent->words;
            while (w)
            {
                strcat(sentence_text, w->word);
                if (w->next)
                {
                    strcat(sentence_text, " ");
                }
                w = w->next;
            }
            
            // Parse to detect multiple sentences
            FileStructure* parsed = parse_text_to_file(sentence_text);
            
            if (parsed && parsed->sentence_count > 0)
            {
                // Step 2: Find the original sentence in the linked list by POINTER
                SentenceNode* prev_sent = NULL;
                SentenceNode* curr_sent = file->sentences;
                
                while (curr_sent && curr_sent != original_sent)
                {
                    prev_sent = curr_sent;
                    curr_sent = curr_sent->next;
                }
                
                if (curr_sent == original_sent)
                {
                    // Step 3: Create new sentence nodes from parsed structure
                    SentenceNode* new_head = NULL;
                    SentenceNode* new_tail = NULL;
                    int new_count = 0;
                    
                    SentenceNode* parsed_curr = parsed->sentences;
                    while (parsed_curr)
                    {
                        SentenceNode* new_sent = create_sentence_node();
                        WordNode* word = parsed_curr->words;
                        while (word)
                        {
                            add_word_to_sentence(new_sent, word->word);
                            word = word->next;
                        }
                        
                        if (!new_head)
                        {
                            new_head = new_sent;
                            new_tail = new_sent;
                        }
                        else
                        {
                            new_tail->next = new_sent;
                            new_tail = new_sent;
                        }
                        
                        new_count++;
                        parsed_curr = parsed_curr->next;
                    }
                    
                    // Step 4: Replace ONLY this sentence - insert new sentences at its position
                    // This PUSHES DOWN all subsequent sentences (including other users' locked sentences)
                    if (new_head)
                    {
                        // Link new sentences to what came AFTER the original
                        new_tail->next = original_sent->next;
                        
                        // Link to what came BEFORE
                        if (prev_sent)
                        {
                            prev_sent->next = new_head;
                        }
                        else
                        {
                            // Original was the first sentence
                            file->sentences = new_head;
                        }
                        
                        // Update sentence count: -1 (removed original) + new_count (added new)
                        file->sentence_count = file->sentence_count - 1 + new_count;
                        
                        log_message("storage_server", "INFO", 
                                   "Replaced sentence %d with %d new sentences in %s",
                                   session->sentence_num, new_count, session->filename);
                    }
                }
                else
                {
                    log_message("storage_server", "ERROR", 
                               "Could not find locked sentence %d in linked list for %s",
                               session->sentence_num, session->filename);
                }
                
                free_file_structure(parsed);
            }
            
            // Step 5: Update file statistics
            file->total_words = 0;
            file->total_chars = 0;
            SentenceNode *s = file->sentences;
            while (s)
            {
                WordNode *word = s->words;
                while (word)
                {
                    if (!(is_sentence_delimiter(word->word[0]) && strlen(word->word) == 1))
                    {
                        file->total_words++;
                    }
                    word = word->next;
                }
                s = s->next;
            }
            
            char *text = file_to_text(file);
            if (text)
            {
                file->total_chars = strlen(text);
                free(text);
            }
            
            // Step 6: Save to disk immediately (this user's changes are committed)
            save_file_to_disk(session->filename, file);
            increment_file_version(session->filename, "system");
            
            log_message("storage_server", "INFO", "ETIRW committed changes to disk for %s", 
                       session->filename);
            
            // Step 6.5: Send metadata update to name server
            if (ss.nm_socket >= 0)
            {
                Packet metadata_pkt = {0};
                metadata_pkt.operation = OP_UPDATE_METADATA;
                metadata_pkt.ss_id = ss.id;
                strncpy(metadata_pkt.filename, session->filename, MAX_FILENAME - 1);
                metadata_pkt.word_index = file->total_words;    // word_count
                metadata_pkt.sentence_num = file->total_chars;  // char_count
                
                if (send_packet(ss.nm_socket, &metadata_pkt) >= 0)
                {
                    log_message("storage_server", "INFO", "Metadata update sent for %s (words=%d, chars=%d)", 
                               session->filename, file->total_words, file->total_chars);
                }
                else
                {
                    log_message("storage_server", "WARNING", "Failed to send metadata update for %s", 
                               session->filename);
                }
            }
            
            // Step 6.6: Notify name server to replicate this write immediately
            if (ss.nm_socket >= 0)
            {
                Packet notify_pkt = {0};
                notify_pkt.operation = OP_REPLICA_WRITE;
                notify_pkt.ss_id = ss.id;
                strncpy(notify_pkt.filename, session->filename, MAX_FILENAME - 1);
                
                // Send notification to name server (fire and forget)
                if (send_packet(ss.nm_socket, &notify_pkt) >= 0)
                {
                    log_message("storage_server", "INFO", "Replication notification sent for %s after ETIRW", 
                               session->filename);
                }
                else
                {
                    log_message("storage_server", "WARNING", "Failed to send replication notification for %s", 
                               session->filename);
                }
            }
        }
        
        // Step 7: Unlock the original sentence (DO NOT FREE - it's part of the cache)
        // The sentence will be freed when the entire file_structure is freed in release_file_cache
        if (original_sent)
        {
            log_message("storage_server", "INFO", 
                       "Unlocking original sentence ptr=%p for %s",
                       (void*)original_sent, session->filename);
            
            pthread_mutex_unlock(&original_sent->lock);
            // DO NOT FREE HERE - it's part of the cached file structure
        }
        
        // Step 8: Free the working copy (this is a separate copy, not part of the cache)
        if (working_sent)
        {
            free_single_sentence(working_sent);
        }
        
        pthread_mutex_unlock(&cache->cache_lock);
        
        // Release the file cache (will free memory if this is the last session)
        release_file_cache(cache_idx);
    }
    else if (session->locked_sentence)
    {
        // Just unlock if no cache
        log_message("storage_server", "INFO", 
                   "Unlocking sentence ptr=%p (no cache case)",
                   (void*)session->locked_sentence);
        
        pthread_mutex_unlock(&session->locked_sentence->lock);
        
        // Free working copy if exists
        if (session->working_copy)
        {
            free_single_sentence(session->working_copy);
        }
    }
    
    session->active = 0;
    session->locked_sentence = NULL;
    session->working_copy = NULL;
    session->cache_index = -1;
    
    pthread_mutex_unlock(&ss.sessions_lock);
}

// Write to file (pointer-based locking with persistent sessions)
