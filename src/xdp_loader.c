#include <signal.h>

#include "../common/xdp_load.h"

static const char* __doc__ = "Program for loading kernel section of the xdp program\n";

static struct xdp_program* prog;

static bool global_exit;

static const struct option_wrapper long_options[] = {

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

static void exit_application(int signal) {
    int err;

    cfg.unload_all = true;
    err = do_unload(&cfg);
    if (err) {
        fprintf(stderr, "Couldn't detach XDP program on iface '%s' : (%d)\n", cfg.ifname, err);
    }

    signal = signal;
    global_exit = true;
}

int main(int argc, char** argv) {
    int err;

    signal(SIGINT, exit_application);

    parse_cmdline_args(argc, argv, long_options, &cfg, __doc__);

    err = load_xdp_program(&cfg, prog);

    if (err) {
        fprintf(stderr, "ERROR: loading program: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }

    while (!global_exit) {
        sleep(1);
    }

    return EXIT_OK;
}