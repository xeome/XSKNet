#pragma once
#include <linux/types.h>
#include <stdbool.h>
#include <xdp/libxdp.h>
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

/* Defined in common_params.o */
extern int verbose;

/* Exit return codes */
#define EXIT_OK 0   /* == EXIT_SUCCESS (stdlib.h) man exit(3) */
#define EXIT_FAIL 1 /* == EXIT_FAILURE (stdlib.h) man exit(3) */
#define EXIT_FAIL_OPTION 2
#define EXIT_FAIL_XDP 30
#define EXIT_FAIL_BPF 40

/* This common_user.h is used by userspace programs */

#include <getopt.h>

struct option_wrapper {
    struct option option;
    char* help;
    char* metavar;
    bool required;
};

void usage(const char* prog_name, const char* doc, const struct option_wrapper* long_options, bool full);

void parse_cmdline_args(int argc,
                        char** argv,
                        const struct option_wrapper* long_options,
                        struct config* cfg,
                        const char* doc,
                        bool user);

struct bpf_object* load_bpf_object_file(const char* filename, int ifindex);
struct xdp_program* load_bpf_and_xdp_attach(struct config* cfg);

const char* action2str(__u32 action);

int check_map_fd_info(const struct bpf_map_info* info, const struct bpf_map_info* exp);

int open_bpf_map_file(const char* pin_dir, const char* mapname, struct bpf_map_info* info);

#include <stdbool.h>

void* tcp_server_nonblocking(void* arg);
char* send_to_daemon(char* msg);

static inline void handle_error(const char* msg);
void handle_client(int client_fd, bool* global_exit);

void create_port(void* arg);
void delete_port(void* arg);

int handle_cmd(char* cmd, void* arg);

int update_devmap(int ifindex);

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

struct poll_arg {
    struct xsk_socket_info* xsk;
    volatile bool* global_exit;
};

void* stats_poll(void* arg);

#define VETH_NUM 100
#include <stdbool.h>

bool create_veth(char* veth_name);
bool delete_veth(char* veth_name);
int add_to_veth_list(char* veth_name);
int remove_from_veth_list(char* veth_name);
char** get_veth_list();
int init_veth_list();

int load_xdp_program(struct config* cfg, struct xdp_program* prog, char* map_name);
int do_unload(struct config* cfg);
void rx_and_process(struct config* cfg, struct xsk_socket_info* xsk_socket, bool* global_exit);

#include <xdp/xsk.h>

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