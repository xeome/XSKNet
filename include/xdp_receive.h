/*
 * File: xdp_receive.h
 * This file contains the implementation of the XDP receiver. The receiver is responsible for receiving packets from the XDP
 * socket and processing them.
 */
#pragma once
void rx_and_process(struct config* cfg, struct xsk_socket_info* xsk_socket, bool* global_exit);