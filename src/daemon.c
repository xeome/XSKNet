#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "args.h"
#include "lwlog.h"
#include "signal_handler.h"
#include "veth_list.h"
#include "socket.h"
#include "socket_handler.h"

int main(const int argc, char* argv[]) {
    options_t options;
    options_parser(argc, argv, &options);

    signal_init();
    const int err = pthread_create(&socket_thread, NULL, socket_server_thread_func, &global_exit_flag);
    if (err != 0) {
        lwlog_crit("pthread_create: %s", strerror(err));
        exit(EXIT_FAILURE);
    }

    veths = veth_list_create(10);

    lwlog_info("Starting Daemon");

    while (!global_exit_flag) {
        sleep(1);
    }

    veth_list_destroy(veths);
    exit_daemon();

    return 0;
}
