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

    lwlog_info("Starting client");

    socket_send_to_port("create_port test", 8080);
    sleep(5);
    socket_send_to_port("delete_port test", 8080);

    return 0;
}
