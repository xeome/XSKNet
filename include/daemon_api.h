#pragma once

void* tcp_server_nonblocking(void* arg);
char* send_to_daemon(char* msg);

static inline void handle_error(const char* msg);
void handle_client(int client_fd);