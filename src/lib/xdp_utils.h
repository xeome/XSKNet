#pragma once

enum {
    EXIT_OK = 0,
    EXIT_FAIL = 1,
    EXIT_FAIL_OPTION = 2,
    EXIT_FAIL_MEM= 5,
    EXIT_FAIL_XDP= 30,
    EXIT_FAIL_BPF= 40,
};

#define pin_basedir "/sys/fs/bpf"