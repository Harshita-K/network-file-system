#ifndef SS_CACHE_H
#define SS_CACHE_H

#include "ss_state.h"

// Cache operations
int get_file_cache(const char* filename);
void release_file_cache(int cache_idx);

// Write session management
ActiveWriteSession* create_write_session(const char* filename, int sentence_num, const char* lock_token, int cache_idx);
ActiveWriteSession* find_write_session(const char* filename, int sentence_num, const char* lock_token);
SentenceNode* duplicate_sentence(SentenceNode *original);
void free_single_sentence(SentenceNode *sentence);
void release_test_lock(ActiveWriteSession* session);
void close_write_session(ActiveWriteSession* session);

#endif // SS_CACHE_H
