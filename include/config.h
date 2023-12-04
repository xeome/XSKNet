#pragma once

#define IF_NAMESIZE 16

struct config {
    enum xdp_attach_mode attach_mode;
    __u32 xdp_flags;
    int ifindex;
    char* ifname;
    char ifname_buf[IF_NAMESIZE];
    int redirect_ifindex;
    char* redirect_ifname;
    char redirect_ifname_buf[IF_NAMESIZE];
    bool do_unload;
    __u32 prog_id;
    bool reuse_maps;
    char pin_dir[512];
    char filename[512];
    char progname[32];
    char src_mac[18];
    char dest_mac[18];
    __u16 xsk_bind_flags;
    int xsk_if_queue;
    bool xsk_poll_mode;
    bool unload_all;
};