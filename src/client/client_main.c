#include "../../include/common.h"
#include "../../include/client_commands.h"
#include "../../include/client_network.h"
#include <readline/readline.h>
#include <readline/history.h>

// Global client state
Client client;

// Main function
int main()
{
    init_client();

    // Get username with retry on duplicate
    int connection_result;
    while (1)
    {
        printf("Enter username: ");
        if (!fgets(client.username, MAX_USERNAME, stdin))
        {
            printf("ERROR: Failed to read username\n");
            return 1;
        }
        client.username[strcspn(client.username, "\n")] = 0;

        if (strlen(client.username) == 0)
        {
            printf("ERROR: Username cannot be empty\n");
            continue;
        }

        // Connect to Name Server
        connection_result = connect_to_nm();
        
        if (connection_result == -2)
        {
            // Duplicate username - retry
            close(client.nm_socket);
            continue;
        }
        else if (connection_result < 0)
        {
            // Other error
            return 1;
        }
        
        // Successfully connected
        break;
    }

    printf("Connected to NFS. Type 'HELP' for available commands.\n");

    // Command loop
    while (client.is_running)
    {
        char *input = readline("nfs> ");

        if (!input)
        {
            break;
        }

        if (strlen(input) > 0)
        {
            add_history(input);
            process_command(input);
        }

        free(input);
    }

    // Cleanup
    close(client.nm_socket);
    printf("Disconnected from NFS.\n");

    return 0;
}
