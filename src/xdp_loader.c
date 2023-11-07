#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <bpf/bpf.h>
#include <xdp/libxdp.h>
#include <xdp/xsk.h>

#include "defs.h"
#include "xdp_load.h"
#include "lwlog.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static const char* pin_basedir = "/sys/fs/bpf";
const char* map_name = "xsks_map";

/* Pinning maps under /sys/fs/bpf in subdir */
int pin_maps_in_bpf_object(struct bpf_object* bpf_obj, const char* subdir) {
    char map_filename[PATH_MAX];
    char pin_dir[PATH_MAX];
    int err, len;

    len = snprintf(pin_dir, PATH_MAX, "%s/%s", pin_basedir, subdir);
    if (len < 0) {
        fprintf(stderr, "ERR: creating pin dirname\n");
        return EXIT_FAIL_OPTION;
    }

    len = snprintf(map_filename, PATH_MAX, "%s/%s/%s", pin_basedir, subdir, map_name);
    if (len < 0) {
        fprintf(stderr, "ERR: creating map_name\n");
        return EXIT_FAIL_OPTION;
    }

    /* Existing/previous XDP prog might not have cleaned up */
    if (access(map_filename, F_OK) != -1) {
        if (verbose)
            printf(" - Unpinning (remove) prev maps in %s/\n", pin_dir);

        /* Basically calls unlink(3) on map_filename */
        err = bpf_object__unpin_maps(bpf_obj, pin_dir);
        if (err) {
            fprintf(stderr, "ERR: UNpinning maps in %s\n", pin_dir);
            return EXIT_FAIL_BPF;
        }
    }
    if (verbose)
        lwlog_info("Pinning maps in %s/", pin_dir);

    /* This will pin all maps in our bpf_object */
    err = bpf_object__pin_maps(bpf_obj, pin_dir);
    if (err)
        return EXIT_FAIL_BPF;

    return 0;
}

int load_xdp_program(struct config* cfg, struct xdp_program* prog, int* xsk_map_fd) {
    int err;
    char errmsg[1024];
    DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts);
    DECLARE_LIBXDP_OPTS(xdp_program_opts, xdp_opts, 0);

    if (cfg->filename[0] != 0) {
        struct bpf_map* map;

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
            lwlog_err("ERR: loading program: %s", errmsg);
            return err;
        }

        err = xdp_program__attach(prog, cfg->ifindex, cfg->attach_mode, 0);
        if (err) {
            libxdp_strerror(err, errmsg, sizeof(errmsg));
            lwlog_err("Couldn't attach XDP program on iface '%s' : %s (%d)", cfg->ifname, errmsg, err);
            return err;
        }

        /* We also need to load the xsks_map */
        map = bpf_object__find_map_by_name(xdp_program__bpf_obj(prog), "xsks_map");
        *xsk_map_fd = bpf_map__fd(map);
        lwlog_info("Found xsks_map with fd %d", *xsk_map_fd);
        if (*xsk_map_fd < 0) {
            lwlog_crit("ERROR: no xsks map found: %s", strerror(*xsk_map_fd));
            exit(EXIT_FAIL);
        }

        /* Pin the maps */
        err = pin_maps_in_bpf_object(xdp_program__bpf_obj(prog), cfg->ifname);
        if (err) {
            lwlog_err("ERR: pinning maps in %s", cfg->ifname);
            return err;
        }
    }

    return 0;
}

int do_unload(struct config* cfg) {
    struct xdp_multiprog* mp = NULL;
    enum xdp_attach_mode mode;
    int err = EXIT_FAILURE;
    DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts);

    mp = xdp_multiprog__get_from_ifindex(cfg->ifindex);
    if (libxdp_get_error(mp)) {
        // fprintf(stderr, "Unable to get xdp_dispatcher program: %s\n", strerror(errno));
        lwlog_warning("Unable to get xdp_dispatcher program: %s", strerror(errno));
        goto out;
    } else if (!mp) {
        // fprintf(stderr, "No XDP program loaded on %s\n", cfg->ifname);
        lwlog_warning("No XDP program loaded on %s", cfg->ifname);
        mp = NULL;
        goto out;
    }

    if (cfg->unload_all) {
        err = xdp_multiprog__detach(mp);
        if (err) {
            // fprintf(stderr, "Unable to detach XDP program: %s\n", strerror(-err));
            lwlog_warning("Unable to detach XDP program: %s", strerror(-err));
            goto out;
        }
    } else {
        struct xdp_program* prog = NULL;

        while ((prog = xdp_multiprog__next_prog(prog, mp))) {
            if (xdp_program__id(prog) == cfg->prog_id) {
                mode = xdp_multiprog__attach_mode(mp);
                goto found;
            }
        }

        if (xdp_multiprog__is_legacy(mp)) {
            prog = xdp_multiprog__main_prog(mp);
            if (xdp_program__id(prog) == cfg->prog_id) {
                mode = xdp_multiprog__attach_mode(mp);
                goto found;
            }
        }

        prog = xdp_multiprog__hw_prog(mp);
        if (xdp_program__id(prog) == cfg->prog_id) {
            mode = XDP_MODE_HW;
            goto found;
        }

        // printf("Program with ID %u not loaded on %s\n", cfg->prog_id, cfg->ifname);
        lwlog_warning("Program with ID %u not loaded on %s", cfg->prog_id, cfg->ifname);
        err = -ENOENT;
        goto out;

    found:
        // printf("Detaching XDP program with ID %u from %s\n", xdp_program__id(prog), cfg->ifname);
        lwlog_info("Detaching XDP program with ID %u from %s", xdp_program__id(prog), cfg->ifname);
        err = xdp_program__detach(prog, cfg->ifindex, mode, 0);
        if (err) {
            // fprintf(stderr, "Unable to detach XDP program: %s\n", strerror(-err));
            lwlog_warning("Unable to detach XDP program: %s", strerror(-err));
            goto out;
        }
    }

out:
    xdp_multiprog__close(mp);
    return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
