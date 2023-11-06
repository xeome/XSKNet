#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <linux/icmpv6.h>
#include <linux/if_ether.h>
#include <linux/if_link.h>
#include <linux/ipv6.h>
#include <net/if.h>

#include <bpf/bpf.h>
#include <xdp/libxdp.h>
#include <xdp/xsk.h>

#include "xdp_daemon.h"
#include "xdp_load.h"
#include "defs.h"
#include "lwlog.h"

#define NUM_FRAMES 4096
#define FRAME_SIZE XSK_UMEM__DEFAULT_FRAME_SIZE
#define RX_BATCH_SIZE 64
#define INVALID_UMEM_FRAME UINT64_MAX

int xsk_map_fd;
static bool global_exit;
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

    {{"poll-mode", no_argument, NULL, 'p'}, "Use the poll() API waiting for packets to arrive"},

    {{"quiet", no_argument, NULL, 'q'}, "Quiet mode (no output)"},

    {{"filename", required_argument, NULL, 1}, "Load program from <file>", "<file>"},

    {{"progname", required_argument, NULL, 2}, "Load program from function <name> in the ELF file", "<name>"},

    {{0, 0, NULL, 0}, NULL, false}};

int main(int argc, char** argv) {
    int err;

    signal(SIGINT, exit_application);

    lwlog_info("Starting XDP Daemon");
    /* Cmdline options can change progname */
    parse_cmdline_args(argc, argv, long_options, &cfg, __doc__);

    /* Required option */
    if (cfg.ifindex == -1) {
        lwlog_crit("ERROR: Required option --dev missing\n\n");
        usage(argv[0], __doc__, long_options, (argc == 1));
        return EXIT_FAIL_OPTION;
    }

    /* Load XDP kernel program */
    err = load_xdp_program(&cfg, prog, &xsk_map_fd);

    if (err) {
        // fprintf(stderr, "ERROR: loading program: %s\n", strerror(err));
        lwlog_crit("ERROR: loading program: %s\n", strerror(err));
        exit(1);
    }

    while (!global_exit) {
        sleep(1);
    }

    return 0;
}

void exit_application(int sig) {
    global_exit = true;
    lwlog_info("Stopping XDP Daemon");
    do_unload(&cfg);
}
