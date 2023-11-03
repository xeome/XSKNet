#pragma once

#include <stdlib.h>

#include <bpf/bpf.h>
#include <xdp/libxdp.h>
#include <xdp/xsk.h>

#include "../common/common_libbpf.h"
#include "../common/common_params.h"
#include "../common/common_user_bpf_xdp.h"

static int xsk_map_fd;
static bool custom_xsk = false;

static struct config cfg = {
    .ifindex = -1,
};

int load_xdp_program(struct config* cfg, struct xdp_program* prog);