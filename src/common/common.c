#include "../../include/common.h"
#include <stdarg.h>

// Error messages mapping
const char *get_error_message(int error_code)
{
    switch (error_code)
    {
    case SUCCESS:
        return "Success";
    case ERR_FILE_NOT_FOUND:
        return "File not found";
    case ERR_PERMISSION_DENIED:
        return "Permission denied";
    case ERR_FILE_EXISTS:
        return "File already exists";
    case ERR_INVALID_OPERATION:
        return "Invalid operation";
    case ERR_SERVER_UNAVAILABLE:
        return "Server unavailable";
    case ERR_NETWORK_ERROR:
        return "Network error";
    case ERR_FILE_LOCKED:
        return "File is locked for writing";
    case ERR_INVALID_SENTENCE:
        return "Invalid sentence number";
    case ERR_INVALID_WORD_INDEX:
        return "Invalid word index";
    case ERR_MEMORY_ERROR:
        return "Memory allocation error";
    case ERR_FOLDER_NOT_FOUND:
        return "Folder not found";
    case ERR_FOLDER_EXISTS:
        return "Folder already exists";
    case ERR_CHECKPOINT_NOT_FOUND:
        return "Checkpoint not found";
    case ERR_REQUEST_EXISTS:
        return "Access request already exists";
    case ERR_USER_NOT_FOUND:
        return "User not found";
    case ERR_REPLICA_UNAVAILABLE:
        return "Replica server unavailable";
    case ERR_SYNC_FAILED:
        return "Synchronization failed";
    case ERR_VERSION_CONFLICT:
        return "Version conflict detected";
    case ERR_USER_ALREADY_LOGGED_IN:
        return "User already logged in";
    case ERR_RESERVED_EXTENSION:
        return "Reserved extension not allowed (.db and .checkpoint are reserved)";
    default:
        return "Unknown error";
    }
}

// Logging function with timestamps
void log_message(const char *component, const char *level, const char *format, ...)
{
    time_t now;
    time(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    char log_filename[MAX_PATH];
    snprintf(log_filename, sizeof(log_filename), "logs/%s.log", component);

    FILE *log_file = fopen(log_filename, "a");
    if (log_file)
    {
        fprintf(log_file, "[%s] [%s] ", timestamp, level);

        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);

        fprintf(log_file, "\n");
        fclose(log_file);
    }

    // Also print to console for important messages
    if (strcmp(level, "ERROR") == 0 || strcmp(level, "INFO") == 0)
    {
        printf("[%s] [%s] ", timestamp, level);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}

// Log request details
void log_request(const char *component, const char *client_ip, int port, const char *operation)
{
    log_message(component, "REQUEST", "From %s:%d - Operation: %s", client_ip, port, operation);
}

// Log response details
void log_response(const char *component, const char *client_ip, int port, int error_code)
{
    log_message(component, "RESPONSE", "To %s:%d - Status: %s",
                client_ip, port, get_error_message(error_code));
}

// Create server socket
int create_server_socket(int port)
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        close(server_fd);
        return -1;
    }

#ifdef SO_REUSEPORT
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt SO_REUSEPORT failed");
        close(server_fd);
        return -1;
    }
#endif

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }

    // Listen
    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

// Connect to server
int connect_to_server(const char *ip, int port)
{
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 address
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection failed");
        close(sock);
        return -1;
    }

    return sock;
}

// Send packet
int send_packet(int socket_fd, Packet *packet)
{
    int total_sent = 0;
    int bytes_to_send = sizeof(Packet);
    char *buffer = (char *)packet;

    while (total_sent < bytes_to_send)
    {
        int sent = send(socket_fd, buffer + total_sent, bytes_to_send - total_sent, 0);
        if (sent < 0)
        {
            perror("Send failed");
            return -1;
        }
        total_sent += sent;
    }

    return total_sent;
}

// Receive packet
int receive_packet(int socket_fd, Packet *packet)
{
    int total_received = 0;
    int bytes_to_receive = sizeof(Packet);
    char *buffer = (char *)packet;

    while (total_received < bytes_to_receive)
    {
        int received = recv(socket_fd, buffer + total_received, bytes_to_receive - total_received, 0);
        if (received < 0)
        {
            perror("Receive failed");
            return -1;
        }
        if (received == 0)
        {
            // Connection closed
            return 0;
        }
        total_received += received;
    }

    return total_received;
}

// Format time
char *format_time(time_t t)
{
    static char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return buffer;
}
