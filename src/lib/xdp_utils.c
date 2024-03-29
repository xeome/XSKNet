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
#include "args.h"

enum { PATH_MAX = 4096 };

/* Pinning maps under /sys/fs/bpf in subdir */
int pin_maps_in_bpf_object(struct bpf_object* bpf_obj, const char* subdir, const char* map_name) {
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
    if (err) {
        lwlog_err("pinning maps in %s", pin_dir);
        return EXIT_FAIL_BPF;
    }

    return 0;
}

int unload_xdp_from_ifname(const char* ifname) {
    if (ifname == NULL) {
        lwlog_err("ifname is NULL");
        return EXIT_FAIL_OPTION;
    }
    lwlog_info("Unloading XDP program from interface %s", ifname);

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

int open_bpf_map_file(const char* pin_dir, const char* mapname, struct bpf_map_info* info) {
    char filename[PATH_MAX];
    __u32 info_len = sizeof(*info);

    const int len = snprintf(filename, PATH_MAX, "%s/%s", pin_dir, mapname);
    if (len < 0) {
        fprintf(stderr, "ERR: constructing full mapname path\n");
        return -1;
    }

    const int fd = bpf_obj_get(filename);
    if (fd < 0) {
        lwlog_err("WARN: Failed to open bpf map file:%s err(%d):%s\n", filename, errno, strerror(errno));
        return fd;
    }

    if (info) {
        const int err = bpf_obj_get_info_by_fd(fd, info, &info_len);
        if (err) {
            lwlog_err("ERR: %s() can't get info - %s\n", __func__, strerror(errno));

            return EXIT_FAIL_BPF;
        }
    }

    return fd;
}

int load_xdp_and_attach_to_ifname(const char* ifname, const char* filename, const char* progname, const char* map_name) {
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

    if (map_name == NULL) {
        lwlog_info("No map name specified for %s, not pinning maps", ifname);
        return 0;
    }

    /* Pin the maps */
    err = pin_maps_in_bpf_object(xdp_program__bpf_obj(prog), ifname, map_name);
    if (err) {
        lwlog_err("ERR: pinning maps in %s", ifname);
        return err;
    }

    return EXIT_OK;
}

int update_devmap(int ifindex, char* ifname) {
    char pin_dir[PATH_MAX] = {0};
    struct bpf_map_info info = {0};

    const int len = snprintf(pin_dir, PATH_MAX, "%s/%s", pin_basedir, opts.dev);
    if (len < 0 || len >= PATH_MAX) {
        lwlog_err("Couldn't format pin_dir");
        return -1;
    }

    const int map_fd = open_bpf_map_file(pin_dir, "xdp_devmap", &info);
    if (map_fd < 0) {
        lwlog_err("Couldn't open xdp_devmap");
        return -1;
    }

    const int key = 0;
    const int ret = bpf_map_update_elem(map_fd, &key, &ifindex, BPF_ANY);
    if (ret) {
        lwlog_info("Couldn't update devmap for %s", ifname);
        return -1;
    }
    return 0;
}