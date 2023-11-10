/*
 * File: xdp_loader.h
 * This file contains the implementation of the XDP loader. The loader is responsible for loading the XDP program to the kernel.
 */

#pragma once

int load_xdp_program(struct config* cfg, struct xdp_program* prog);
int do_unload(struct config* cfg);