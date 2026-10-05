#ifndef NS_FILE_OPS_H
#define NS_FILE_OPS_H

#include "ns_state.h"

// File management
int find_file_index_locked(const char *filename);
int find_file_index(const char *filename);
int get_ss_for_file(const char *filename);
int check_access(const char *filename, const char *username, int required_permission);

// Lock management
void generate_lock_token(char *token, const char *filename, int sentence_num, const char *username);
int acquire_sentence_lock(const char *filename, int sentence_num, const char *username, char *lock_token);
int release_sentence_lock(const char *filename, int sentence_num, const char *lock_token);

// File operation handlers
int handle_create(Packet *request, Packet *response);
int handle_delete(Packet *request, Packet *response);
int handle_view(Packet *request, Packet *response);
int handle_info(Packet *request, Packet *response);
int handle_exec(Packet *request, Packet *response);
int handle_create_folder(Packet *request, Packet *response);
int handle_move_file(Packet *request, Packet *response);
int handle_view_folder(Packet *request, Packet *response);
int handle_update_metadata(Packet *request, Packet *response);

// Access control handlers
int handle_add_access(Packet *request, Packet *response);
int handle_remove_access(Packet *request, Packet *response);
int handle_request_access(Packet *request, Packet *response);
int handle_list_requests(Packet *request, Packet *response);
int handle_approve_request(Packet *request, Packet *response);
int handle_deny_request(Packet *request, Packet *response);

// User management
int user_exists(const char *username);
int handle_list_users(Packet *request, Packet *response);

#endif // NS_FILE_OPS_H
