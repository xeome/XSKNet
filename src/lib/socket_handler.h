//
// Created by xeome on 1/6/24.
//

#ifndef SOCKET_HANDLER_H
#define SOCKET_HANDLER_H

int handle_command(const char* command, const void* data);

void handle_client(const int client_socket_fd);

void send_command(const int socket_fd, const char* command);

#endif //SOCKET_HANDLER_H
