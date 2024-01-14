#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket_handler.h"
#include "socket_cmds.h"
#include "lwlog.h"

enum { CMD_SIZE = 1024 };

typedef void (*CommandHandler)(const char* data);

typedef struct {
    char* command;
    CommandHandler handler;
} Command;

Command commands[] = {
    {"create_port", create_port},
    {"delete_port", delete_port},
};

int handle_command(const char* command, const void* data) {
    for (size_t i = 0; i < sizeof(commands) / sizeof(Command); i++) {
        if (strcmp(command, commands[i].command) == 0) {
            commands[i].handler(data);
            return 0;
        }
    }
    return -1;
}

void handle_client(const int client_socket_fd) {
    char buffer[CMD_SIZE];
    long bytes_read;
    while ((bytes_read = read(client_socket_fd, buffer, sizeof(buffer))) > 0) {
        buffer[bytes_read] = '\0';
        lwlog_info("Received: %s", buffer);
        const char* command = strtok(buffer, " ");
        const char* data = strtok(NULL, "");
        if (handle_command(command, data) < 0) {
            lwlog_err("Unknown command: %s", command);
        }
    }
    close(client_socket_fd);
}
