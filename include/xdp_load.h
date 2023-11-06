#pragma once

int load_xdp_program(struct config* cfg, struct xdp_program* prog, int* xsk_map_fd);
int do_unload(struct config* cfg);