#include <net/if.h>

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <pthread.h>

#include "libxsk.h"
#include "lwlog.h"

bool global_exit;
struct xdp_program* prog;
pthread_t socket_thread;
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

void exit_application(int signal);
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

void unload_xdp_program(const char* ifname) {
    lwlog_info("Unloading XDP program from %s", ifname);

    cfg.ifindex = if_nametoindex(ifname);
    if (cfg.ifindex == 0) {
        lwlog_err("Couldn't get ifindex for %s", ifname);
        return;
    }

    lwlog_info("Unloading XDP program");

    int err = do_unload(&cfg);
    if (err) {
        lwlog_err("Couldn't detach XDP program on iface '%s' : (%d)", cfg.ifname, err);
    }

    if (!delete_veth(ifname)) {
        lwlog_err("Couldn't delete veth pair");
    }
}

void exit_application(int signal) {
    cfg.unload_all = true;
    global_exit = true;

    char** veth_list = get_veth_list();
    for (int i = 0; i < VETH_NUM; i++) {
        if (veth_list[i] != NULL) {
            unload_xdp_program(veth_list[i]);
        }
    }

    pthread_join(socket_thread, NULL);
    lwlog_info("Exiting XDP Daemon");
}
