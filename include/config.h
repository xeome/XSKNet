#pragma once

#define IF_NAMESIZE 16

struct config {
    enum xdp_attach_mode attach_mode;
    __u16 xsk_bind_flags;
    char* filename;
    char* progname;
    char* ifname;
    char ifname_buf[IF_NAMESIZE];
    int xsk_if_queue;
    int ifindex;
    int prog_id;
    bool unload_all;
    bool do_unload;
    __u32 xdp_flags;
};
void init_empty_config(struct config* cfg);