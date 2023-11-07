/*
 * File: xdp_daemon.h
 * This file contains the implementation of the XDP daemon. The daemon is responsible for loading the XDP program and setting up
 * the XDP socket.
 */
#pragma once
#include <stdio.h>
#include "common_params.h"

void exit_application(int sig);
void* tcp_server_nonblocking();

void handle_client(int client_fd);
void handle_error(const char* msg);