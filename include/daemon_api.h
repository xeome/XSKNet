#pragma once

#include <stdbool.h>

void* tcp_server_nonblocking(void* arg);
char* send_to_daemon(char* msg);

static inline void handle_error(const char* msg);
void handle_client(int client_fd, bool* global_exit);

void create_port(void* arg);
void delete_port(void* arg);

int handlecmd(char* cmd, void* arg);