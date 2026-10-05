#ifndef CLIENT_COMMANDS_H
#define CLIENT_COMMANDS_H

#include "common.h"

// Client State Structure
typedef struct
{
    char username[MAX_USERNAME];
    int nm_socket;
    int is_running;
} Client;

// Global client state (defined in client_network.c)
extern Client client;

// Command handlers
void cmd_view(char *args);
void cmd_read(char *filename);
void cmd_create(char *filename);
void cmd_write(char *filename, char *sentence_num_str);
void cmd_undo(char *filename);
void cmd_info(char *filename);
void cmd_delete(char *filename);
void cmd_create_folder(char *folder_path);
void cmd_move_file(char *filename, char *folder_path);
void cmd_view_folder(char *folder_path);
void cmd_checkpoint(char *filename, char *tag);
void cmd_view_checkpoint(char *filename, char *tag);
void cmd_revert_checkpoint(char *filename, char *tag);
void cmd_list_checkpoints(char *filename);
void cmd_request_access(char *filename, char *permission);
void cmd_list_requests();
void cmd_approve_request(char *filename, char *requester);
void cmd_deny_request(char *filename, char *requester);
void cmd_stream(char *filename);
void cmd_list();
void cmd_add_access(char *flag, char *filename, char *target_user);
void cmd_remove_access(char *filename, char *target_user);
void cmd_exec(char *filename);

// Command processor
void process_command(char *input);

// Utility functions
int parse_flags(const char *flag_str);

#endif // CLIENT_COMMANDS_H
