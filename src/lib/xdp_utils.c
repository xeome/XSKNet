#include <stdlib.h>
#include <unistd.h>

#include <bpf/libbpf.h> /* bpf_get_link_xdp_id + bpf_set_link_xdp_id */
#include <string.h>     /* strerror */
#include <net/if.h>     /* IF_NAMESIZE */
#include <errno.h>
#include <bpf/bpf.h>
#include <xdp/libxdp.h>

#include <linux/if_link.h> /* Need XDP flags */
#include "lwlog.h"
#include "xdp_utils.h"

enum { PATH_MAX = 4096 };

/* Pinning maps under /sys/fs/bpf in subdir */
int pin_maps_in_bpf_object(struct bpf_object* bpf_obj, const char* subdir, char* map_name) {
    if (bpf_obj == NULL) {
        lwlog_err("bpf_obj is NULL");
        return EXIT_FAIL_OPTION;
    }

    if (subdir == NULL) {
        lwlog_err("subdir is NULL");
        return EXIT_FAIL_OPTION;
    }

    if (map_name == NULL) {
        lwlog_err("map_name is NULL");
        return EXIT_FAIL_OPTION;
    }

    char map_filename[PATH_MAX];
    char pin_dir[PATH_MAX];
    int err;

    int len = snprintf(pin_dir, PATH_MAX, "%s/%s", pin_basedir, subdir);
    if (len < 0) {
        lwlog_err("creating pin dirname");
        return EXIT_FAIL_OPTION;
    }

    len = snprintf(map_filename, PATH_MAX, "%s/%s/%s", pin_basedir, subdir, map_name);
    if (len < 0) {
        lwlog_err("creating map_name");
        return EXIT_FAIL_OPTION;
    }
    /* Existing/previous XDP prog might not have cleaned up */
    if (access(map_filename, F_OK) != -1) {
        lwlog_info("Unpinning (remove) prev maps in %s/", pin_dir);

        /* Basically calls unlink(3) on map_filename */
        err = bpf_object__unpin_maps(bpf_obj, pin_dir);
        if (err) {
            lwlog_err("UNpinning maps in %s", pin_dir);
            return EXIT_FAIL_BPF;
        }
    }
    lwlog_info("Pinning maps in %s/", pin_dir);

    /* This will pin all maps in our bpf_object */
    err = bpf_object__pin_maps(bpf_obj, pin_dir);
    if (err)
        return EXIT_FAIL_BPF;

    return 0;
}

int unload_xdp_from_ifname(const char* ifname) {
    if (ifname == NULL) {
        lwlog_err("ifname is NULL");
        return EXIT_FAIL_OPTION;
    }

    struct xdp_multiprog* mp = NULL;
    int err = EXIT_FAILURE;

    DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts);

    int ifindex = if_nametoindex(ifname);
    if (!ifindex) {
        lwlog_err("if_nametoindex(%s)", ifname);
        return EXIT_FAIL_OPTION;
    }

    mp = xdp_multiprog__get_from_ifindex(ifindex);
    if (libxdp_get_error(mp)) {
        lwlog_warning("Unable to get xdp_multiprog program for ifname %s", ifname);
        goto defer;
    } else if (!mp) {
        lwlog_warning("No xdp program found for ifname %s", ifname);
        mp = NULL;
        goto defer;
    }

    err = xdp_multiprog__detach(mp);
    if (err) {
        lwlog_err("xdp_detach failed (err=%d)", err);
    }

defer:
    xdp_multiprog__close(mp);
    return err ? EXIT_FAIL_XDP : EXIT_OK;
}

int load_xdp_and_attach_to_ifname(const char* ifname, const char* filename, const char* progname) {
    lwlog_info("Loading XDP program %s on interface %s", filename, ifname);
    if (ifname == NULL) {
        lwlog_err("ifname is NULL");
        return EXIT_FAIL_OPTION;
    }

    if (filename == NULL) {
        lwlog_err("filename is NULL");
        return EXIT_FAIL_OPTION;
    }

    if (progname == NULL) {
        lwlog_err("progname is NULL");
        return EXIT_FAIL_OPTION;
    }

    DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts);
    DECLARE_LIBXDP_OPTS(xdp_program_opts, xdp_opts, 0);

    xdp_opts.open_filename = filename;
    xdp_opts.prog_name = progname;
    xdp_opts.opts = &opts;

    struct xdp_program* prog = xdp_program__create(&xdp_opts);
    int err = libxdp_get_error(prog);
    if (err) {
        char errmsg[1024];
        libxdp_strerror(err, errmsg, sizeof(errmsg));
        lwlog_err("loading program: %s\n", errmsg);
        exit(EXIT_FAIL_BPF);
    }
    int ifindex = if_nametoindex(ifname);

    err = xdp_program__attach(prog, ifindex, 0, 0);
    if (err) {
        lwlog_err("loading program: %s\n", strerror(-err));
        return EXIT_FAIL_BPF;
    }

    const int prog_fd = xdp_program__fd(prog);
    if (prog_fd < 0) {
        lwlog_err("xdp_program__fd failed: %s\n", strerror(errno));
        return EXIT_FAIL_BPF;
    }

    return EXIT_OK;
}