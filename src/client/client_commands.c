#include "../../include/common.h"
#include "../../include/client_commands.h"
#include "../../include/ai_write.h"
#include <ctype.h>
#include <strings.h>

// Parse flags from command
int parse_flags(const char *flag_str)
{
    int flags = 0;
    if (strchr(flag_str, 'a'))
        flags |= 1; // -a flag
    if (strchr(flag_str, 'l'))
        flags |= 2; // -l flag
    return flags;
}
void cmd_view(char *args)
{
    Packet request = {0};
    request.operation = OP_VIEW;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    request.flags = 0;

    // Parse flags
    if (args && strlen(args) > 0)
    {
        if (args[0] == '-')
        {
            request.flags = parse_flags(args + 1);
        }
    }

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("%s", response.data);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// READ command
void cmd_read(char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        printf("Usage: READ <filename>\n");
        return;
    }

    // Request SS info from NM
    Packet request = {0};
    request.operation = OP_READ;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code != SUCCESS)
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
        return;
    }

    // Connect to SS directly
    int ss_socket = connect_to_server(response.ss_ip, response.ss_port);
    if (ss_socket < 0)
    {
        printf("ERROR: Failed to connect to Storage Server\n");
        return;
    }

    // Send read request to SS
    send_packet(ss_socket, &request);

    Packet ss_response;
    receive_packet(ss_socket, &ss_response);

    if (ss_response.error_code == SUCCESS)
    {
        printf("%s\n", ss_response.data);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(ss_response.error_code));
    }

    close(ss_socket);
}

// CREATE command
void cmd_create(char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        printf("Usage: CREATE <filename>\n");
        return;
    }

    Packet request = {0};
    request.operation = OP_CREATE;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("File created successfully\n");
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// WRITE command
void cmd_write(char *filename, char *sentence_num_str)
{
    if (!filename || !sentence_num_str)
    {
        printf("Usage: WRITE <filename> <sentence_number>\n");
        return;
    }

    int sentence_num = atoi(sentence_num_str);
    if (sentence_num < 1)
    {
        printf("ERROR: Invalid sentence number\n");
        return;
    }

    // Step 1: Acquire lock from Name Server
    Packet lock_request = {0};
    lock_request.operation = OP_ACQUIRE_LOCK;
    strncpy(lock_request.username, client.username, MAX_USERNAME - 1);
    strncpy(lock_request.filename, filename, MAX_FILENAME - 1);
    lock_request.sentence_num = sentence_num;

    send_packet(client.nm_socket, &lock_request);

    Packet lock_response;
    receive_packet(client.nm_socket, &lock_response);

    if (lock_response.error_code != SUCCESS)
    {
        printf("ERROR: %s\n", get_error_message(lock_response.error_code));
        return;
    }

    // Got token from name server
    char lock_token[64];
    strncpy(lock_token, lock_response.lock_token, 64);

    // Step 2: Connect to SS using info from lock response
    int ss_socket = connect_to_server(lock_response.ss_ip, lock_response.ss_port);
    if (ss_socket < 0)
    {
        printf("ERROR: Failed to connect to Storage Server\n");
        return;
    }

    // Step 3: Try to acquire the lock by sending a test WRITE to storage server
    Packet lock_test = {0};
    lock_test.operation = OP_WRITE;
    strncpy(lock_test.username, client.username, MAX_USERNAME - 1);
    strncpy(lock_test.filename, filename, MAX_FILENAME - 1);
    lock_test.sentence_num = sentence_num;
    lock_test.word_index = 0;  // Dummy write at position 0
    strcpy(lock_test.data, "");  // Empty content to test lock
    strncpy(lock_test.lock_token, lock_token, 64);

    send_packet(ss_socket, &lock_test);

    Packet lock_test_resp;
    receive_packet(ss_socket, &lock_test_resp);

    if (lock_test_resp.error_code != SUCCESS)
    {
        if (lock_test_resp.error_code == ERR_FILE_LOCKED)
        {
            printf("ERROR: Sentence %d is currently locked by another user.\n", sentence_num);
        }
        else
        {
            printf("ERROR: %s\n", get_error_message(lock_test_resp.error_code));
        }
        close(ss_socket);
        return;
    }

    // Lock successfully acquired!
    printf("Lock acquired for sentence %d (token: %s)\n", sentence_num, lock_token);
    printf("Enter word updates (format: <word_index> <content>). Type 'ETIRW' to finish:\n");

    int word_index = 0;
    char line[MAX_BUFFER_SIZE];
    int write_count = 1;  // Count the initial test write

    while (1)
    {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin))
            break;

        // Remove newline
        line[strcspn(line, "\n")] = 0;

        if (strcmp(line, "ETIRW") == 0)
        {
            break;
        }

        // Parse word_index and content
        char *token = strtok(line, " ");
        if (!token)
            continue;

        word_index = atoi(token);

        char *content = strtok(NULL, "");
        if (!content)
            continue;

        // Step 4: Send write request with lock token
        Packet write_req = {0};
        write_req.operation = OP_WRITE;
        strncpy(write_req.username, client.username, MAX_USERNAME - 1);
        strncpy(write_req.filename, filename, MAX_FILENAME - 1);
        write_req.sentence_num = sentence_num;
        write_req.word_index = word_index;
        strncpy(write_req.data, content, MAX_BUFFER_SIZE - 1);
        strncpy(write_req.lock_token, lock_token, 64);

        send_packet(ss_socket, &write_req);

        Packet write_resp;
        receive_packet(ss_socket, &write_resp);

        if (write_resp.error_code == SUCCESS)
        {
            printf("Updated successfully\n");
            write_count++;
        }
        else
        {
            printf("ERROR: %s\n", get_error_message(write_resp.error_code));
        }
    }

    // Step 5: Release the lock on storage server first (to clean up session)
    Packet ss_unlock_req = {0};
    ss_unlock_req.operation = OP_RELEASE_LOCK;
    strncpy(ss_unlock_req.filename, filename, MAX_FILENAME - 1);
    ss_unlock_req.sentence_num = sentence_num;
    strncpy(ss_unlock_req.lock_token, lock_token, 64);
    send_packet(ss_socket, &ss_unlock_req);
    
    Packet ss_unlock_resp;
    receive_packet(ss_socket, &ss_unlock_resp);

    close(ss_socket);
    
    // Step 5: Release the lock on name server (to update lock registry)
    Packet unlock_req = {0};
    unlock_req.operation = OP_RELEASE_LOCK;
    strncpy(unlock_req.filename, filename, MAX_FILENAME - 1);
    unlock_req.sentence_num = sentence_num;
    strncpy(unlock_req.lock_token, lock_token, 64);
    send_packet(client.nm_socket, &unlock_req);
    
    Packet unlock_resp;
    receive_packet(client.nm_socket, &unlock_resp);
    
    if (write_count == 0)
    {
        printf("Write operation completed. Lock released (no changes made)\n");
    }
    else
    {
        printf("Write operation completed with %d update(s). Lock released.\n", write_count);
    }
}

// UNDO command
void cmd_undo(char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        printf("Usage: UNDO <filename>\n");
        return;
    }

    // Get SS info
    Packet request = {0};
    request.operation = OP_UNDO;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code != SUCCESS)
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
        return;
    }

    // Connect to SS
    int ss_socket = connect_to_server(response.ss_ip, response.ss_port);
    if (ss_socket < 0)
    {
        printf("ERROR: Failed to connect to Storage Server\n");
        return;
    }

    send_packet(ss_socket, &request);

    Packet ss_response;
    receive_packet(ss_socket, &ss_response);

    if (ss_response.error_code == SUCCESS)
    {
        printf("Undo successful\n");
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(ss_response.error_code));
    }

    close(ss_socket);
}

// INFO command
void cmd_info(char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        printf("Usage: INFO <filename>\n");
        return;
    }

    Packet request = {0};
    request.operation = OP_INFO;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("%s\n", response.data);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// DELETE command
void cmd_delete(char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        printf("Usage: DELETE <filename>\n");
        return;
    }

    Packet request = {0};
    request.operation = OP_DELETE;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("File deleted successfully\n");
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: CREATEFOLDER command
void cmd_create_folder(char *folder_path)
{
    if (!folder_path || strlen(folder_path) == 0)
    {
        printf("Usage: CREATEFOLDER <folder_path>\n");
        return;
    }

    Packet request = {0};
    request.operation = OP_CREATE_FOLDER;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.folder_path, folder_path, MAX_PATH - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("Folder created successfully: %s\n", folder_path);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: MOVE command
void cmd_move_file(char *filename, char *folder_path)
{
    if (!filename || !folder_path || strlen(filename) == 0 || strlen(folder_path) == 0)
    {
        printf("Usage: MOVE <filename> <folder_path>\n");
        return;
    }

    Packet request = {0};
    request.operation = OP_MOVE_FILE;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);
    strncpy(request.folder_path, folder_path, MAX_PATH - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("File moved successfully to %s\n", folder_path);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: VIEWFOLDER command
void cmd_view_folder(char *folder_path)
{
    if (!folder_path || strlen(folder_path) == 0)
    {
        folder_path = ""; // Default to root (empty string)
    }

    Packet request = {0};
    request.operation = OP_VIEW_FOLDER;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.folder_path, folder_path, MAX_PATH - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("%s\n", response.data);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: CHECKPOINT command
void cmd_checkpoint(char *filename, char *tag)
{
    if (!filename || !tag || strlen(filename) == 0 || strlen(tag) == 0)
    {
        printf("Usage: CHECKPOINT <filename> <tag>\n");
        return;
    }

    // First get SS info from name server
    Packet ns_request = {0};
    ns_request.operation = OP_READ; // Just need SS location
    strncpy(ns_request.username, client.username, MAX_USERNAME - 1);
    strncpy(ns_request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &ns_request);

    Packet ns_response;
    receive_packet(client.nm_socket, &ns_response);

    if (ns_response.error_code != SUCCESS)
    {
        printf("ERROR: %s\n", get_error_message(ns_response.error_code));
        return;
    }

    // Connect to storage server
    int ss_socket = connect_to_server(ns_response.ss_ip, ns_response.ss_port);
    if (ss_socket < 0)
    {
        printf("ERROR: Failed to connect to storage server\n");
        return;
    }

    // Send checkpoint request to SS
    Packet request = {0};
    request.operation = OP_CHECKPOINT;
    strncpy(request.filename, filename, MAX_FILENAME - 1);
    strncpy(request.checkpoint_tag, tag, MAX_TAG_LENGTH - 1);

    send_packet(ss_socket, &request);

    Packet response;
    receive_packet(ss_socket, &response);
    close(ss_socket);

    if (response.error_code == SUCCESS)
    {
        printf("Checkpoint created: %s\n", tag);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: VIEWCHECKPOINT command
void cmd_view_checkpoint(char *filename, char *tag)
{
    if (!filename || !tag || strlen(filename) == 0 || strlen(tag) == 0)
    {
        printf("Usage: VIEWCHECKPOINT <filename> <tag>\n");
        return;
    }

    // First get SS info from name server
    Packet ns_request = {0};
    ns_request.operation = OP_READ;
    strncpy(ns_request.username, client.username, MAX_USERNAME - 1);
    strncpy(ns_request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &ns_request);

    Packet ns_response;
    receive_packet(client.nm_socket, &ns_response);

    if (ns_response.error_code != SUCCESS)
    {
        printf("ERROR: %s\n", get_error_message(ns_response.error_code));
        return;
    }

    // Connect to storage server
    int ss_socket = connect_to_server(ns_response.ss_ip, ns_response.ss_port);
    if (ss_socket < 0)
    {
        printf("ERROR: Failed to connect to storage server\n");
        return;
    }

    // Send view checkpoint request
    Packet request = {0};
    request.operation = OP_VIEW_CHECKPOINT;
    strncpy(request.filename, filename, MAX_FILENAME - 1);
    strncpy(request.checkpoint_tag, tag, MAX_TAG_LENGTH - 1);

    send_packet(ss_socket, &request);

    Packet response;
    receive_packet(ss_socket, &response);
    close(ss_socket);

    if (response.error_code == SUCCESS)
    {
        printf("Checkpoint '%s' content:\n%s\n", tag, response.data);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: REVERT command
void cmd_revert_checkpoint(char *filename, char *tag)
{
    if (!filename || !tag || strlen(filename) == 0 || strlen(tag) == 0)
    {
        printf("Usage: REVERT <filename> <tag>\n");
        return;
    }

    // First get SS info from name server
    Packet ns_request = {0};
    ns_request.operation = OP_WRITE; // Need write permission
    strncpy(ns_request.username, client.username, MAX_USERNAME - 1);
    strncpy(ns_request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &ns_request);

    Packet ns_response;
    receive_packet(client.nm_socket, &ns_response);

    if (ns_response.error_code != SUCCESS)
    {
        printf("ERROR: %s\n", get_error_message(ns_response.error_code));
        return;
    }

    // Connect to storage server
    int ss_socket = connect_to_server(ns_response.ss_ip, ns_response.ss_port);
    if (ss_socket < 0)
    {
        printf("ERROR: Failed to connect to storage server\n");
        return;
    }

    // Send revert request
    Packet request = {0};
    request.operation = OP_REVERT_CHECKPOINT;
    strncpy(request.filename, filename, MAX_FILENAME - 1);
    strncpy(request.checkpoint_tag, tag, MAX_TAG_LENGTH - 1);

    send_packet(ss_socket, &request);

    Packet response;
    receive_packet(ss_socket, &response);
    close(ss_socket);

    if (response.error_code == SUCCESS)
    {
        printf("File reverted to checkpoint: %s\n", tag);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: LISTCHECKPOINTS command
void cmd_list_checkpoints(char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        printf("Usage: LISTCHECKPOINTS <filename>\n");
        return;
    }

    // First get SS info from name server
    Packet ns_request = {0};
    ns_request.operation = OP_READ;
    strncpy(ns_request.username, client.username, MAX_USERNAME - 1);
    strncpy(ns_request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &ns_request);

    Packet ns_response;
    receive_packet(client.nm_socket, &ns_response);

    if (ns_response.error_code != SUCCESS)
    {
        printf("ERROR: %s\n", get_error_message(ns_response.error_code));
        return;
    }

    // Connect to storage server
    int ss_socket = connect_to_server(ns_response.ss_ip, ns_response.ss_port);
    if (ss_socket < 0)
    {
        printf("ERROR: Failed to connect to storage server\n");
        return;
    }

    // Send list checkpoints request
    Packet request = {0};
    request.operation = OP_LIST_CHECKPOINTS;
    strncpy(request.filename, filename, MAX_FILENAME - 1);

    send_packet(ss_socket, &request);

    Packet response;
    receive_packet(ss_socket, &response);
    close(ss_socket);

    if (response.error_code == SUCCESS)
    {
        printf("%s\n", response.data);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: REQUESTACCESS command
void cmd_request_access(char *filename, char *permission)
{
    if (!filename || !permission || strlen(filename) == 0 || strlen(permission) == 0)
    {
        printf("Usage: REQUESTACCESS <filename> <READ|WRITE>\n");
        return;
    }

    int perm = (strcasecmp(permission, "READ") == 0) ? ACCESS_READ : ACCESS_WRITE;

    Packet request = {0};
    request.operation = OP_REQUEST_ACCESS;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);
    request.flags = perm;

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("Access request sent for %s\n", filename);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: LISTREQUESTS command
void cmd_list_requests()
{
    Packet request = {0};
    request.operation = OP_LIST_REQUESTS;
    strncpy(request.username, client.username, MAX_USERNAME - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("%s\n", response.data);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: APPROVE command
void cmd_approve_request(char *filename, char *requester)
{
    if (!filename || !requester || strlen(filename) == 0 || strlen(requester) == 0)
    {
        printf("Usage: APPROVE <filename> <requester_username>\n");
        return;
    }

    Packet request = {0};
    request.operation = OP_APPROVE_REQUEST;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);
    strncpy(request.data, requester, MAX_USERNAME - 1); // Pass requester in data field

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("Access request approved for %s\n", requester);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// BONUS FEATURE: DENY command
void cmd_deny_request(char *filename, char *requester)
{
    if (!filename || !requester || strlen(filename) == 0 || strlen(requester) == 0)
    {
        printf("Usage: DENY <filename> <requester_username>\n");
        return;
    }

    Packet request = {0};
    request.operation = OP_DENY_REQUEST;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);
    strncpy(request.data, requester, MAX_USERNAME - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("Access request denied for %s\n", requester);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// STREAM command
void cmd_stream(char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        printf("Usage: STREAM <filename>\n");
        return;
    }

    // Get SS info
    Packet request = {0};
    request.operation = OP_STREAM;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code != SUCCESS)
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
        return;
    }

    // Connect to SS
    int ss_socket = connect_to_server(response.ss_ip, response.ss_port);
    if (ss_socket < 0)
    {
        printf("ERROR: Failed to connect to Storage Server\n");
        return;
    }

    send_packet(ss_socket, &request);

    printf("Streaming: ");
    fflush(stdout);

    while (1)
    {
        Packet stream_resp;
        int received = receive_packet(ss_socket, &stream_resp);

        if (received <= 0)
        {
            printf("\nERROR: Storage Server disconnected\n");
            break;
        }

        if (stream_resp.operation == OP_ACK && strcmp(stream_resp.data, "STOP") == 0)
        {
            printf("\n");
            break;
        }

        if (stream_resp.operation == OP_STREAM && stream_resp.error_code == SUCCESS)
        {
            printf("%s ", stream_resp.data);
            fflush(stdout);
        }
        else if (stream_resp.operation == OP_ERROR)
        {
            printf("\nERROR: %s\n", get_error_message(stream_resp.error_code));
            break;
        }
    }

    close(ss_socket);
}

// LIST command
void cmd_list()
{
    Packet request = {0};
    request.operation = OP_LIST_USERS;
    strncpy(request.username, client.username, MAX_USERNAME - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("Users:\n%s", response.data);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// ADDACCESS command
void cmd_add_access(char *flag, char *filename, char *target_user)
{
    if (!flag || !filename || !target_user)
    {
        printf("Usage: ADDACCESS -R/-W <filename> <username>\n");
        return;
    }

    int permission = ACCESS_READ;
    if (strcmp(flag, "-W") == 0 || strcmp(flag, "-w") == 0)
    {
        permission = ACCESS_WRITE;
    }

    Packet request = {0};
    request.operation = OP_ADD_ACCESS;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);
    strncpy(request.data, target_user, MAX_BUFFER_SIZE - 1);
    request.flags = permission;

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("Access granted successfully\n");
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// REMACCESS command
void cmd_remove_access(char *filename, char *target_user)
{
    if (!filename || !target_user)
    {
        printf("Usage: REMACCESS <filename> <username>\n");
        return;
    }

    Packet request = {0};
    request.operation = OP_REM_ACCESS;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);
    strncpy(request.data, target_user, MAX_BUFFER_SIZE - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("Access removed successfully\n");
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// EXEC command
void cmd_exec(char *filename)
{
    if (!filename || strlen(filename) == 0)
    {
        printf("Usage: EXEC <filename>\n");
        return;
    }

    Packet request = {0};
    request.operation = OP_EXEC;
    strncpy(request.username, client.username, MAX_USERNAME - 1);
    strncpy(request.filename, filename, MAX_FILENAME - 1);

    send_packet(client.nm_socket, &request);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code == SUCCESS)
    {
        printf("Output:\n%s\n", response.data);
    }
    else
    {
        printf("ERROR: %s\n", get_error_message(response.error_code));
    }
}

// Process command
void process_command(char *input)
{
    if (!input || strlen(input) == 0)
        return;

    char *command = strtok(input, " ");
    if (!command)
        return;

    // Convert to uppercase for comparison
    for (int i = 0; command[i]; i++)
    {
        command[i] = toupper(command[i]);
    }

    if (strcmp(command, "VIEW") == 0)
    {
        char *args = strtok(NULL, "");
        cmd_view(args);
    }
    else if (strcmp(command, "READ") == 0)
    {
        char *filename = strtok(NULL, " ");
        cmd_read(filename);
    }
    else if (strcmp(command, "CREATE") == 0)
    {
        char *filename = strtok(NULL, " ");
        cmd_create(filename);
    }
    else if (strcmp(command, "WRITE") == 0)
    {
        char *filename = strtok(NULL, " ");
        char *sentence_num = strtok(NULL, " ");
        char *flag = strtok(NULL, " ");
        
        // Check if -AI flag is present
        if (flag && strcmp(flag, "-AI") == 0) {
            cmd_write_ai(filename, sentence_num, client.nm_socket, client.username);
        } else {
            cmd_write(filename, sentence_num);
        }
    }
    else if (strcmp(command, "UNDO") == 0)
    {
        char *filename = strtok(NULL, " ");
        cmd_undo(filename);
    }
    else if (strcmp(command, "INFO") == 0)
    {
        char *filename = strtok(NULL, " ");
        cmd_info(filename);
    }
    else if (strcmp(command, "DELETE") == 0)
    {
        char *filename = strtok(NULL, " ");
        cmd_delete(filename);
    }
    else if (strcmp(command, "STREAM") == 0)
    {
        char *filename = strtok(NULL, " ");
        cmd_stream(filename);
    }
    else if (strcmp(command, "LIST") == 0)
    {
        cmd_list();
    }
    else if (strcmp(command, "ADDACCESS") == 0)
    {
        char *flag = strtok(NULL, " ");
        char *filename = strtok(NULL, " ");
        char *username = strtok(NULL, " ");
        cmd_add_access(flag, filename, username);
    }
    else if (strcmp(command, "REMACCESS") == 0)
    {
        char *filename = strtok(NULL, " ");
        char *username = strtok(NULL, " ");
        cmd_remove_access(filename, username);
    }
    else if (strcmp(command, "EXEC") == 0)
    {
        char *filename = strtok(NULL, " ");
        cmd_exec(filename);
    }
    else if (strcmp(command, "CREATEFOLDER") == 0)
    {
        char *folder_path = strtok(NULL, " ");
        cmd_create_folder(folder_path);
    }
    else if (strcmp(command, "MOVE") == 0)
    {
        char *filename = strtok(NULL, " ");
        char *folder_path = strtok(NULL, " ");
        cmd_move_file(filename, folder_path);
    }
    else if (strcmp(command, "VIEWFOLDER") == 0)
    {
        char *folder_path = strtok(NULL, " ");
        cmd_view_folder(folder_path);
    }
    else if (strcmp(command, "CHECKPOINT") == 0)
    {
        char *filename = strtok(NULL, " ");
        char *tag = strtok(NULL, " ");
        cmd_checkpoint(filename, tag);
    }
    else if (strcmp(command, "VIEWCHECKPOINT") == 0)
    {
        char *filename = strtok(NULL, " ");
        char *tag = strtok(NULL, " ");
        cmd_view_checkpoint(filename, tag);
    }
    else if (strcmp(command, "REVERT") == 0)
    {
        char *filename = strtok(NULL, " ");
        char *tag = strtok(NULL, " ");
        cmd_revert_checkpoint(filename, tag);
    }
    else if (strcmp(command, "LISTCHECKPOINTS") == 0)
    {
        char *filename = strtok(NULL, " ");
        cmd_list_checkpoints(filename);
    }
    else if (strcmp(command, "REQUESTACCESS") == 0)
    {
        char *filename = strtok(NULL, " ");
        char *permission = strtok(NULL, " ");
        cmd_request_access(filename, permission);
    }
    else if (strcmp(command, "LISTREQUESTS") == 0)
    {
        cmd_list_requests();
    }
    else if (strcmp(command, "APPROVE") == 0)
    {
        char *filename = strtok(NULL, " ");
        char *requester = strtok(NULL, " ");
        cmd_approve_request(filename, requester);
    }
    else if (strcmp(command, "DENY") == 0)
    {
        char *filename = strtok(NULL, " ");
        char *requester = strtok(NULL, " ");
        cmd_deny_request(filename, requester);
    }
    else if (strcmp(command, "EXIT") == 0 || strcmp(command, "QUIT") == 0)
    {
        client.is_running = 0;
    }
    else if (strcmp(command, "HELP") == 0)
    {
        printf("Available commands:\n");
        printf("  VIEW [-a] [-l] [-al]  - List files\n");
        printf("  READ <filename>       - Read file content\n");
        printf("  CREATE <filename>     - Create new file\n");
        printf("  WRITE <file> <sent#>  - Write to file\n");
        printf("  UNDO <filename>       - Undo last change\n");
        printf("  INFO <filename>       - Get file info\n");
        printf("  DELETE <filename>     - Delete file\n");
        printf("  STREAM <filename>     - Stream file content\n");
        printf("  LIST                  - List all users\n");
        printf("  ADDACCESS -R/-W <file> <user> - Add access\n");
        printf("  REMACCESS <file> <user>        - Remove access\n");
        printf("  EXEC <filename>       - Execute file as commands\n");
        printf("\n=== BONUS FEATURES ===\n");
        printf("  CREATEFOLDER <path>              - Create folder\n");
        printf("  MOVE <file> <folder>             - Move file to folder\n");
        printf("  VIEWFOLDER <path>                - View folder contents\n");
        printf("  CHECKPOINT <file> <tag>          - Save checkpoint\n");
        printf("  VIEWCHECKPOINT <file> <tag>      - View checkpoint\n");
        printf("  REVERT <file> <tag>              - Revert to checkpoint\n");
        printf("  LISTCHECKPOINTS <file>           - List checkpoints\n");
        printf("  REQUESTACCESS <file> <READ|WRITE> - Request access\n");
        printf("  LISTREQUESTS                     - List access requests\n");
        printf("  APPROVE <file> <user>            - Approve request\n");
        printf("  DENY <file> <user>               - Deny request\n");
        printf("  EXIT                  - Exit client\n");
    }
    else
    {
        printf("Unknown command. Type 'HELP' for list of commands.\n");
    }
}
