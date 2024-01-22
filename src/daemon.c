#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "args.h"
#include "lwlog.h"
#include "signal_handler.h"
#include "veth_list.h"
#include "socket.h"
#include "socket_handler.h"
#include "xdp_utils.h"

int main(const int argc, char* argv[]) {
    options_parser(argc, argv, &opts);

    signal_init();
    lwlog_info("Starting Daemon");
    veths = veth_list_create(10);

    int err = pthread_create(&socket_thread, NULL, socket_server_thread_func, &global_exit_flag);
    if (err != 0) {
        lwlog_crit("pthread_create: %s", strerror(err));
        exit(EXIT_FAILURE);
    }

    err = load_xdp_and_attach_to_ifname(opts.dev, "obj/phy_xdp.o", "xdp_redirect");
    if (err != EXIT_OK) {
        lwlog_crit("load_xdp_and_attach_to_ifname: %s", strerror(err));
    }

    while (!global_exit_flag) {
        sleep(1);
    }

    lwlog_info("Stopping Daemon");
    veth_list_destroy(veths);

    exit_daemon();

    return 0;
}
