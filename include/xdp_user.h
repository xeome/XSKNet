#pragma once

const struct option_wrapper long_options[] = {
    {{"help", no_argument, NULL, 'h'}, "Show help", false},
    {{"dev", required_argument, NULL, 'd'}, "Operate on device <ifname>", "<ifname>", true},
    {{"poll-mode", no_argument, NULL, 'p'}, "Use the poll() API waiting for packets to arrive"},
    {{"quiet", no_argument, NULL, 'q'}, "Quiet mode (no output)"},
    {{0, 0, NULL, 0}, NULL, false}};
void request_port(char* ifname);
void request_port_deletion(char* ifname);

void cleanup(struct xsk_socket_info* xsk_socket);
void start_stats_thread(struct xsk_socket_info* xsk_socket, pthread_t* stats_poll_thread);
static void set_memory_limit();