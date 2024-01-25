#pragma once

int handle_command(const char* command, void* data);

void handle_client(const int client_socket_fd);

void send_command(const int socket_fd, const char* command);
