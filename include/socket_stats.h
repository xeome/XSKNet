struct poll_arg {
    struct xsk_socket_info* xsk;
    volatile bool* global_exit;
};

void* stats_poll(void* arg);