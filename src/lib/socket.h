#pragma once

#include <pthread.h>

extern int client_socket_fd;
extern int server_socket_fd;

extern pthread_t socket_thread;

void* socket_server_thread_func(void* exit_flag);
void socket_send_to_port(char* cmd, int port);
void request_port(const char* veth_name);
void remove_port(const char* veth_name);
void request_phy_ifname(char* veth_name);