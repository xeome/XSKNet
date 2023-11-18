#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <pthread.h>

#include "xdp_daemon.h"
#include "xdp_loader.h"
#include "lwlog.h"
#include "daemon_api.h"
#include "xdp_daemon_utils.h"

bool global_exit;
struct xdp_program* prog;

static struct config cfg = {
    .ifindex = -1,
    .unload_all = true,
};

static const char* __doc__ = "AF_XDP kernel bypass example\n";

pthread_t socket_thread;
int main(int argc, char** argv) {
    int err;
    global_exit = false;

    signal(SIGINT, exit_application);
    signal(SIGTERM, exit_application);

    lwlog_info("Starting XDP Daemon");

    init_veth_list();

    /* Cmdline options can change progname */
    parse_cmdline_args(argc, argv, long_options, &cfg, __doc__, false);

    /* Load XDP program */
    if (load_xdp_program(&cfg, prog, "xdp_devmap") != 0) {
        lwlog_err("Couldn't load XDP program");
    }

    err = add_to_veth_list(cfg.ifname);
    if (err) {
        lwlog_err("Couldn't add veth to list");
    }

    /* Start socket (Polling and non-blocking) */
    err = pthread_create(&socket_thread, NULL, tcp_server_nonblocking, &global_exit);
    if (err) {
        lwlog_crit("ERROR: Failed creating socket thread \"%s\"\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (!global_exit) {
        sleep(1);
    }

    return 0;
}

void exit_application(int signal) {
    int err;

    cfg.unload_all = true;
    global_exit = true;

    int i = 0;
    char** veth_list = get_veth_list();
    while (veth_list[i] != NULL) {
        lwlog_info("Unloading XDP program from %s", veth_list[i]);
        cfg.ifindex = if_nametoindex(veth_list[i]);
        if (cfg.ifindex == 0) {
            lwlog_err("Couldn't get ifindex for %s", veth_list[i]);
            return;
        }

        lwlog_info("Unloading XDP program");

        // Use do_unload
        err = do_unload(&cfg);
        if (err) {
            lwlog_err("Couldn't detach XDP program on iface '%s' : (%d)", cfg.ifname, err);
        }

        if (!delete_veth(veth_list[i])) {
            lwlog_err("Couldn't delete veth pair");
        }

        i++;
    }

    pthread_join(socket_thread, NULL);
    lwlog_info("Exiting XDP Daemon");
}
