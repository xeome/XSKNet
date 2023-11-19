#include <stdlib.h>
#include <unistd.h>

#include "defs.h"
#include "lwlog.h"
#include "common_defines.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Pinning maps under /sys/fs/bpf in subdir */
int pin_maps_in_bpf_object(struct bpf_object* bpf_obj, const char* subdir, char* map_name) {
    char map_filename[PATH_MAX];
    char pin_dir[PATH_MAX];
    int err, len;

    len = snprintf(pin_dir, PATH_MAX, "%s/%s", pin_basedir, subdir);
    if (len < 0) {
        lwlog_err("ERR: creating pin dirname");
        return EXIT_FAIL_OPTION;
    }

    len = snprintf(map_filename, PATH_MAX, "%s/%s/%s", pin_basedir, subdir, map_name);
    if (len < 0) {
        lwlog_err("ERR: creating map_name");
        return EXIT_FAIL_OPTION;
    }

    /* Existing/previous XDP prog might not have cleaned up */
    if (access(map_filename, F_OK) != -1) {
        if (verbose)
            lwlog_info("Unpinning (remove) prev maps in %s/", pin_dir);

        /* Basically calls unlink(3) on map_filename */
        err = bpf_object__unpin_maps(bpf_obj, pin_dir);
        if (err) {
            lwlog_err("ERR: UNpinning maps in %s", pin_dir);
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

int load_xdp_program(struct config* cfg, struct xdp_program* prog, char* map_name) {
    int err;
    char errmsg[1024];
    DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts);
    DECLARE_LIBXDP_OPTS(xdp_program_opts, xdp_opts, 0);

    if (cfg->filename[0] != 0) {
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

        lwlog_info("Loading XDP program from %s to ifindex %d and ifname %s", cfg->filename, cfg->ifindex, cfg->ifname);
        err = xdp_program__attach(prog, cfg->ifindex, cfg->attach_mode, 0);
        if (err) {
            libxdp_strerror(err, errmsg, sizeof(errmsg));
            lwlog_err("Couldn't attach XDP program on iface '%s' : %s (%d)", cfg->ifname, errmsg, err);
            return err;
        }

        /* Pin the maps */
        err = pin_maps_in_bpf_object(xdp_program__bpf_obj(prog), cfg->ifname, map_name);
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
    lwlog_info("Getting XDP program from ifname %s and ifindex %d", cfg->ifname, cfg->ifindex);
    if (libxdp_get_error(mp)) {
        lwlog_warning("Unable to get xdp_dispatcher program");
        goto out;
    } else if (!mp) {
        lwlog_warning("No XDP program loaded on %s", cfg->ifname);
        mp = NULL;
        goto out;
    }

    if (cfg->unload_all) {
        err = xdp_multiprog__detach(mp);
        if (err) {
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

        lwlog_warning("Program with ID %u not loaded on %s", cfg->prog_id, cfg->ifname);
        err = -ENOENT;
        goto out;

    found:
        lwlog_info("Detaching XDP program with ID %u from %s", xdp_program__id(prog), cfg->ifname);
        err = xdp_program__detach(prog, cfg->ifindex, mode, 0);
        if (err) {
            lwlog_warning("Unable to detach XDP program: %s", strerror(-err));
            goto out;
        }
    }

out:
    xdp_multiprog__close(mp);
    return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
