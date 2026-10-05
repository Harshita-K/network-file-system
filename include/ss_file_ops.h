#ifndef SS_FILE_OPS_H
#define SS_FILE_OPS_H

#include "ss_state.h"

// File operations
int handle_create_file(const char *filename, const char *owner);
int handle_delete_file(const char *filename);
int handle_read_file(const char *filename, char *content);
int handle_write_file(const char *filename, int sentence_num, int word_index, const char *content, const char *lock_token);
void handle_stream_file(int client_socket, const char *filename);
int handle_file_info(const char *filename, Packet *response);

// Undo operations
int get_file_index_for_undo(const char *filename);
void push_undo(const char *filename, FileStructure *file);
FileStructure *pop_undo(const char *filename);
int handle_undo(const char *filename);

#endif // SS_FILE_OPS_H
