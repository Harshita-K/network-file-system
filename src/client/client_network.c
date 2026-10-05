#include "../../include/common.h"
#include "../../include/client_commands.h"

// Initialize client
void init_client()
{
    client.is_running = 1;
    client.nm_socket = -1;

    log_message("client", "INFO", "Client initialized");
}

// Connect to Name Server
int connect_to_nm()
{
    client.nm_socket = connect_to_server(NAME_SERVER_IP, NAME_SERVER_PORT);
    if (client.nm_socket < 0)
    {
        printf("ERROR: Failed to connect to Name Server\n");
        return -1;
    }

    // Register with Name Server
    Packet reg_packet = {0};
    reg_packet.operation = OP_REGISTER_CLIENT;
    strncpy(reg_packet.username, client.username, MAX_USERNAME - 1);
    strcpy(reg_packet.data, "127.0.0.1");
    reg_packet.ss_port = 0; // Client port not needed

    send_packet(client.nm_socket, &reg_packet);

    Packet response;
    receive_packet(client.nm_socket, &response);

    if (response.error_code != SUCCESS)
    {
        if (response.error_code == ERR_USER_ALREADY_LOGGED_IN)
        {
            printf("ERROR: User '%s' is already logged in\n", client.username);
            return -2;  // Special return code for duplicate username
        }
        printf("ERROR: Failed to register with Name Server: %s\n", get_error_message(response.error_code));
        return -1;
    }

    log_message("client", "INFO", "Connected to Name Server as %s", client.username);
    return 0;
}
