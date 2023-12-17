#pragma once

#include <linux/types.h>
#include <stdbool.h>
#include <xdp/libxdp.h>
#include <xdp/xsk.h>
#include <getopt.h>

#include "config.h"

extern int verbose;

/* Exit return codes */
#define EXIT_OK 0   /* == EXIT_SUCCESS (stdlib.h) man exit(3) */
#define EXIT_FAIL 1 /* == EXIT_FAILURE (stdlib.h) man exit(3) */
#define EXIT_FAIL_OPTION 2
#define EXIT_FAIL_MEM 5
#define EXIT_FAIL_XDP 30
#define EXIT_FAIL_BPF 40

#define VETH_NUM 100

// xdp utils
struct xdp_program* load_bpf_and_xdp_attach(const struct config* cfg);
struct bpf_object* load_bpf_object_file(const char* filename, int ifindex);
const char* action2str(__u32 action);
int check_map_fd_info(const struct bpf_map_info* info, const struct bpf_map_info* exp);
int open_bpf_map_file(const char* pin_dir, const char* mapname, struct bpf_map_info* info);
int load_xdp_program(const struct config* cfg, struct xdp_program* prog, char* map_name);

// api commands
void* tcp_server_nonblocking(void* arg);
char* send_to_daemon(const char* msg);
static inline void handle_error(const char* msg);
void handle_client(int client_fd, const bool* global_exit);
void create_port(const void* arg);
void delete_port(const void* arg);
int handle_cmd(char* cmd, void* arg);
int update_devmap(int ifindex, char* ifname);
int do_unload(const struct config* cfg);

struct poll_arg {
    struct xsk_socket_info* xsk;
    volatile bool* global_exit;
};

void* stats_poll(void* arg);

// veth utils
struct veth_pair {
    char* veth_outer;
    char* veth_inner;
    int outer_ifindex;
    int inner_ifindex;
};

int init_veth_list();
int add_to_veth_list(const char* prefix);
int remove_from_veth_list(const char* veth_outer);
void create_veth(const char* veth_outer, const char* veth_inner);
void delete_veth(const char* veth_outer);
struct veth_pair** get_veth_list();
struct veth_pair* get_index(int index);
struct veth_pair* get_pair(const char* prefix);

struct tx_if {
    __u8 ifindex;
    __u8 mac[6];
};

void rx_and_process(struct config* cfg, struct xsk_socket_info* xsk_socket, const bool* global_exit, struct tx_if* egress);

#define NUM_FRAMES 4096
#define FRAME_SIZE XSK_UMEM__DEFAULT_FRAME_SIZE
#define RX_BATCH_SIZE 64
#define INVALID_UMEM_FRAME UINT64_MAX

struct xsk_umem_info {
    struct xsk_ring_prod fq;
    struct xsk_ring_cons cq;
    struct xsk_umem* umem;
    void* buffer;
};
struct stats_record {
    uint64_t timestamp;
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t tx_packets;
    uint64_t tx_bytes;
};
struct xsk_socket_info {
    struct xsk_ring_cons rx;
    struct xsk_ring_prod tx;
    struct xsk_umem_info* umem;
    struct xsk_socket* xsk;

    uint64_t umem_frame_addr[NUM_FRAMES];
    uint32_t umem_frame_free;

    uint32_t outstanding_tx;

    struct stats_record stats;
    struct stats_record prev_stats;
};

struct xsk_socket_info* init_xsk_socket(struct config* cfg);
