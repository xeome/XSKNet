#pragma once

static const char* pin_basedir = "/sys/fs/bpf";
/* Defined in common_params.o */
extern int verbose;

/* Exit return codes */
#define EXIT_OK 0   /* == EXIT_SUCCESS (stdlib.h) man exit(3) */
#define EXIT_FAIL 1 /* == EXIT_FAILURE (stdlib.h) man exit(3) */
#define EXIT_FAIL_OPTION 2
#define EXIT_FAIL_MEM 5
#define EXIT_FAIL_XDP 30
#define EXIT_FAIL_BPF 40
