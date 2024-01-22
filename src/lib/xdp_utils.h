#pragma once

#include <linux/bpf.h>

enum {
    EXIT_OK = 0,
    EXIT_FAIL = 1,
    EXIT_FAIL_OPTION = 2,
    EXIT_FAIL_MEM = 5,
    EXIT_FAIL_XDP = 30,
    EXIT_FAIL_BPF = 40,
};

#define pin_basedir "/sys/fs/bpf"
int unload_xdp_from_ifname(const char* ifname);
int open_bpf_map_file(const char* pin_dir, const char* mapname, struct bpf_map_info* info);
int load_xdp_and_attach_to_ifname(const char* ifname, const char* filename, const char* progname);