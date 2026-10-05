#ifndef NS_CLIENT_H
#define NS_CLIENT_H

#include "ns_state.h"

// Client management
int register_client(Packet *packet, int client_socket);

// Client request handler
void *handle_client_request(void *arg);

#endif // NS_CLIENT_H
