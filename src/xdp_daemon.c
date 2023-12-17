#include <net/if.h>

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <pthread.h>

#include "libxsk.h"
#include "lwlog.h"
#include "flags.h"

bool global_exit;
struct xdp_program* prog;
pthread_t socket_thread;

static const char* __doc__ = "AF_XDP kernel bypass example\n";

const struct option_wrapper long_options[] = {
    {{"help", no_argument, NULL, 'h'}, "Show help", false},
    {{"dev", required_argument, NULL, 'd'}, "Operate on device <ifname>", "<ifname>", true},
    {{"filename", required_argument, NULL, 1}, "Load program from <file>", "<file>"},
    {{"progname", required_argument, NULL, 2}, "Load program from function <name> in the ELF file", "<name>"},
    {{0, 0, NULL, 0}, NULL, false}};

void exit_application(int signal);
int main(const int argc, char** argv) {
    global_exit = false;

    signal(SIGINT, exit_application);
    signal(SIGTERM, exit_application);

    init_veth_list();
    lwlog_info("Starting XDP Daemon");

    struct config* cfg = malloc(sizeof(struct config));
    /* Cmdline options can change progname */
    parse_cmdline_args(argc, argv, long_options, cfg, __doc__, false);
    /* Load XDP program */
    if (load_xdp_program(cfg, prog, "xdp_devmap") != 0) {
        lwlog_err("Couldn't load XDP program");
    }
    /* Start socket (Polling and non-blocking) */
    const int err = pthread_create(&socket_thread, NULL, tcp_server_nonblocking, &global_exit);
    if (err) {
        lwlog_crit("ERROR: Failed creating socket thread \"%s\"\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (!global_exit) {
        sleep(1);
    }

    return 0;
}

void unload_xdp_program(const char* ifname) {
    if (ifname == NULL) {
        lwlog_err("ifname is NULL");
        return;
    }

    lwlog_info("Unloading XDP program from %s", ifname);
    struct config* cfg = malloc(sizeof(struct config));
    init_empty_config(cfg);
    cfg->ifname = strdup(ifname);

    cfg->ifindex = if_nametoindex(ifname);
    if (cfg->ifindex == 0) {
        lwlog_err("Couldn't get ifindex for %s", ifname);
        return;
    }

    const int err = do_unload(cfg);
    if (err) {
        lwlog_err("Couldn't detach XDP program on iface '%s' : (%d)", cfg->ifname, err);
    }

    free(cfg);
}

void exit_application(int signal) {
    global_exit = true;

    struct veth_pair** veth_list = get_veth_list();
    for (int i = 0; i < VETH_NUM; i++) {
        if (veth_list[i] != NULL) {
            unload_xdp_program(veth_list[i]->veth_outer);
            unload_xdp_program(veth_list[i]->veth_inner);
            delete_veth(veth_list[i]->veth_outer);  // it will delete veth_inner too
        }
    }

    unload_xdp_program(phy_ifname);
    pthread_join(socket_thread, NULL);
    lwlog_info("Exiting XDP Daemon");
}
