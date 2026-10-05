#include "../../include/ns_state.h"
#include "../../include/ns_trie.h"
#include "../../include/ns_file_ops.h"
#include "../../include/ns_storage.h"
#include "../../include/ns_client.h"
#include <signal.h>

// Global name server state
NameServer ns;

void init_name_server()
{
    ns.ss_count = 0;
    ns.client_count = 0;
    ns.file_count = 0;
    ns.cache_index = 0;
    ns.folder_count = 0;
    ns.access_requests = NULL;
    ns.is_running = 1;

    ns.file_trie = create_trie_node();

    pthread_mutex_init(&ns.ss_lock, NULL);
    pthread_mutex_init(&ns.client_lock, NULL);
    pthread_mutex_init(&ns.file_lock, NULL);
    pthread_mutex_init(&ns.cache_lock, NULL);
    pthread_mutex_init(&ns.folder_lock, NULL);
    pthread_mutex_init(&ns.request_lock, NULL);

    for (int i = 0; i < CACHE_SIZE; i++)
    {
        ns.cache[i].valid = 0;
    }

    log_message("name_server", "INFO", "Name Server initialized");
}

// Generate unique lock token
void *heartbeat_monitor_thread(void *arg)
{
    log_message("name_server", "INFO", "Heartbeat monitor started (passive mode)");
    
    while (ns.is_running)
    {
        sleep(HEARTBEAT_INTERVAL);
        
        time_t current_time = time(NULL);
        
        pthread_mutex_lock(&ns.ss_lock);
        
        for (int i = 0; i < ns.ss_count; i++)
        {
            StorageServerInfo *ss = &ns.storage_servers[i];
            
            // Check if SS has timed out (hasn't sent heartbeat in HEARTBEAT_TIMEOUT seconds)
            if (ss->is_active && (current_time - ss->last_heartbeat > HEARTBEAT_TIMEOUT))
            {
                ss->is_active = 0;
                log_message("name_server", "WARNING", "Storage Server %d marked as INACTIVE (no heartbeat for %ld seconds)", 
                           ss->id, current_time - ss->last_heartbeat);
                printf("Storage Server %d is now INACTIVE (timeout)\n", ss->id);
            }
        }
        
        pthread_mutex_unlock(&ns.ss_lock);
    }
    
    log_message("name_server", "INFO", "Heartbeat monitor stopped");
    return NULL;
}

void signal_handler(int sig)
{
    log_message("name_server", "INFO", "Shutting down...");
    ns.is_running = 0;
    close(ns.server_socket);
    exit(0);
}

// Main function
int main()
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    init_name_server();

    ns.server_socket = create_server_socket(NAME_SERVER_PORT);
    if (ns.server_socket < 0)
    {
        log_message("name_server", "ERROR", "Failed to create server socket");
        return 1;
    }

    log_message("name_server", "INFO", "Name Server listening on port %d", NAME_SERVER_PORT);
    printf("Name Server started on port %d\n", NAME_SERVER_PORT);

    // Start heartbeat monitoring thread
    pthread_t heartbeat_tid;
    if (pthread_create(&heartbeat_tid, NULL, heartbeat_monitor_thread, NULL) != 0)
    {
        log_message("name_server", "ERROR", "Failed to create heartbeat monitor thread");
    }
    else
    {
        pthread_detach(heartbeat_tid);
        log_message("name_server", "INFO", "Heartbeat monitor thread started");
    }

    while (ns.is_running)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int *client_socket = malloc(sizeof(int));
        *client_socket = accept(ns.server_socket, (struct sockaddr *)&client_addr, &addr_len);

        if (*client_socket < 0)
        {
            free(client_socket);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client_request, client_socket);
        pthread_detach(tid);
    }

    // Cleanup
    free_trie(ns.file_trie);
    close(ns.server_socket);

    return 0;
}
