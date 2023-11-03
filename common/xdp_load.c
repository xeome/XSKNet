#include "xdp_load.h"

int load_xdp_program(struct config* cfg, struct xdp_program* prog) {
    int err;
    char errmsg[1024];
    DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts);
    DECLARE_LIBXDP_OPTS(xdp_program_opts, xdp_opts, 0);

    if (cfg->filename[0] != 0) {
        struct bpf_map* map;

        custom_xsk = true;
        xdp_opts.open_filename = cfg->filename;
        xdp_opts.prog_name = cfg->progname;
        xdp_opts.opts = &opts;

        if (cfg->progname[0] != 0) {
            prog = xdp_program__create(&xdp_opts);
        } else {
            prog = xdp_program__open_file(cfg->filename, NULL, &opts);
        }

        err = libxdp_get_error(prog);
        if (err) {
            libxdp_strerror(err, errmsg, sizeof(errmsg));
            fprintf(stderr, "ERR: loading program: %s\n", errmsg);
            return err;
        }

        err = xdp_program__attach(prog, cfg->ifindex, cfg->attach_mode, 0);
        if (err) {
            libxdp_strerror(err, errmsg, sizeof(errmsg));
            fprintf(stderr, "Couldn't attach XDP program on iface '%s' : %s (%d)\n", cfg->ifname, errmsg, err);
            return err;
        }

        /* We also need to load the xsks_map */
        map = bpf_object__find_map_by_name(xdp_program__bpf_obj(prog), "xsks_map");
        xsk_map_fd = bpf_map__fd(map);
        if (xsk_map_fd < 0) {
            fprintf(stderr, "ERROR: no xsks map found: %s\n", strerror(xsk_map_fd));
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}