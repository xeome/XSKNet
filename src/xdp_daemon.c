#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>

#include <sys/resource.h>
#include <pthread.h>

#include <bpf/bpf.h>
#include <xdp/xsk.h>

#include "xdp_daemon.h"
#include "xdp_loader.h"
#include "xdp_socket.h"
#include "xdp_receive.h"
#include "lwlog.h"
#include "socket_stats.h"
#include "socket99.h"
#include "daemon_api.h"
#include "xdp_daemon_utils.h"

bool global_exit;
struct xdp_program* prog;

static struct config cfg = {
    .ifindex = -1,
    .unload_all = true,
};

static const char* __doc__ = "AF_XDP kernel bypass example\n";

const struct option_wrapper long_options[] = {

    {{"help", no_argument, NULL, 'h'}, "Show help", false},

    {{"dev", required_argument, NULL, 'd'}, "Operate on device <ifname>", "<ifname>", true},

    {{"skb-mode", no_argument, NULL, 'S'}, "Install XDP program in SKB (AKA generic) mode"},

    {{"native-mode", no_argument, NULL, 'N'}, "Install XDP program in native mode"},

    {{"auto-mode", no_argument, NULL, 'A'}, "Auto-detect SKB or native mode"},

    {{"force", no_argument, NULL, 'F'}, "Force install, replacing existing program on interface"},

    {{"copy", no_argument, NULL, 'c'}, "Force copy mode"},

    {{"zero-copy", no_argument, NULL, 'z'}, "Force zero-copy mode"},

    {{"queue", required_argument, NULL, 'Q'}, "Configure interface receive queue for AF_XDP, default=0"},

    {{"quiet", no_argument, NULL, 'q'}, "Quiet mode (no output)"},

    {{"filename", required_argument, NULL, 1}, "Load program from <file>", "<file>"},

    {{"progname", required_argument, NULL, 2}, "Load program from function <name> in the ELF file", "<name>"},

    {{0, 0, NULL, 0}, NULL, false}};

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

    /* Start socket (Polling and non-blocking) */
    err = pthread_create(&socket_thread, NULL, tcp_server_nonblocking, &global_exit);
    if (err) {
        lwlog_crit("ERROR: Failed creating socket thread \"%s\"\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (!global_exit) {
        sleep(1);
    }

    /* Required option */
    // if (cfg.ifindex == -1) {
    //     lwlog_crit("ERROR: Required option --dev missing\n\n");
    //     usage(argv[0], __doc__, long_options, (argc == 1));
    //     return EXIT_FAIL_OPTION;
    // }

    return 0;
}

void exit_application(int signal) {
    int err;

    cfg.unload_all = true;
    global_exit = true;
    // err = do_unload(&cfg);
    // if (err) {
    //     lwlog_err("Couldn't detach XDP program on iface '%s' : (%d)", cfg.ifname, err);
    // }

    // /* Delete the veth pair */
    // lwlog_info("Deleting veth pair");
    // if (!delete_veth(cfg.ifname)) {
    //     lwlog_err("Couldn't delete veth pair");
    // }

    // Unload XDP programs from all interfaces in veth_list
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
