#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "args.h"
#include "lwlog.h"
#include "signal_handler.h"
#include "veth_list.h"
#include "socket.h"
#include "socket_handler.h"

int main(const int argc, char* argv[]) {
    options_parser(argc, argv, &opts);

    if (strcmp(opts.dev, "/dev/stdout") == 0) {
        lwlog_err("Cannot use /dev/stdout as a device name");
        exit(EXIT_FAILURE);
    }

    client_signal_init();

    lwlog_info("Starting client");

    request_port(opts.dev);
    sleep(5);
    remove_port(opts.dev);

    return 0;
}
