#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>

#include "args.h"
#include "lwlog.h"
#include "signal_handler.h"
#include "veth_list.h"
#include "socket.h"
#include "socket_handler.h"
#include "xsk_utils.h"
#include "xsk_stats.h"
#include "xsk_receive.h"

int main(const int argc, char* argv[]) {
    options_parser(argc, argv, &opts);

    if (strcmp(opts.dev, "/dev/stdout") == 0) {
        lwlog_err("Cannot use /dev/stdout as a device name");
        exit(EXIT_FAILURE);
    }

    client_signal_init();

    lwlog_info("Starting client");

    request_port(opts.dev);

    char phy_ifname[IFNAMSIZ];
    request_phy_ifname(phy_ifname);
    lwlog_info("Phy interface name: %s", phy_ifname);

    set_memory_limit();

    struct egress_sock ingress;
    init_iface(&ingress, phy_ifname);

    struct xsk_socket_info* xsk_socket = init_xsk_socket(opts.dev);
    if (xsk_socket == NULL) {
        lwlog_crit("init_xsk_socket: %s", strerror(errno));
    }

    pthread_t stats_poll_thread;
    int err = pthread_create(&stats_poll_thread, NULL, stats_poll, xsk_socket);
    if (err != 0) {
        lwlog_crit("pthread_create: %s", strerror(err));
    }

    rx_and_process(xsk_socket, &global_exit_flag, &ingress);

    sleep(5);
    remove_port(opts.dev);

    return 0;
}
