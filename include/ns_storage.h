#ifndef NS_STORAGE_H
#define NS_STORAGE_H

#include "ns_state.h"

// Storage server management
int register_storage_server(Packet *packet, const char *actual_ip);

// Replication
void *async_replicate_create(void *arg);
void *async_replicate_write(void *arg);
void *async_replicate_checkpoint(void *arg);

// Monitoring
void *heartbeat_monitor_thread(void *arg);

#endif // NS_STORAGE_H
