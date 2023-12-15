#include <stdlib.h>
#include <unistd.h>

#include <bpf/libbpf.h> /* bpf_get_link_xdp_id + bpf_set_link_xdp_id */
#include <string.h>     /* strerror */
#include <net/if.h>     /* IF_NAMESIZE */
#include <errno.h>
#include <bpf/bpf.h>
#include <xdp/libxdp.h>

#include <linux/if_link.h> /* Need XDP flags */

#include "libxsk.h"
#include "lwlog.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// static int reuse_maps(struct bpf_object* obj, const char* path) {
//     struct bpf_map* map;

//     if (!obj)
//         return -ENOENT;

//     if (!path)
//         return -EINVAL;

//     bpf_object__for_each_map(map, obj) {
//         int len, err;
//         int pinned_map_fd;
//         char buf[PATH_MAX];

//         len = snprintf(buf, PATH_MAX, "%s/%s", path, bpf_map__name(map));
//         if (len < 0) {
//             return -EINVAL;
//         } else if (len >= PATH_MAX) {
//             return -ENAMETOOLONG;
//         }

//         pinned_map_fd = bpf_obj_get(buf);
//         if (pinned_map_fd < 0)
//             return pinned_map_fd;

//         err = bpf_map__reuse_fd(map, pinned_map_fd);
//         if (err)
//             return err;
//     }

//     return 0;
// }

#if 0
struct bpf_object *load_bpf_object_file_reuse_maps(const char *file,
						   int ifindex,
						   const char *pin_dir)
{
	int err;
	struct bpf_object *obj;

	obj = open_bpf_object(file, ifindex);
	if (!obj) {
		fprintf(stderr, "ERR: failed to open object %s\n", file);
		return NULL;
	}

	err = reuse_maps(obj, pin_dir);
	if (err) {
		fprintf(stderr, "ERR: failed to reuse maps for object %s, pin_dir=%s\n",
				file, pin_dir);
		return NULL;
	}

	err = bpf_object__load(obj);
	if (err) {
		fprintf(stderr, "ERR: loading BPF-OBJ file(%s) (%d): %s\n",
			file, err, strerror(-err));
		return NULL;
	}

	return obj;
}
#endif

struct xdp_program* load_bpf_and_xdp_attach(const struct config* cfg) {
    /* In next assignment this will be moved into ../common/ */

    DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts);
    DECLARE_LIBXDP_OPTS(xdp_program_opts, xdp_opts, 0);

    xdp_opts.open_filename = cfg->filename;
    xdp_opts.prog_name = cfg->progname;
    xdp_opts.opts = &opts;

    struct xdp_program* prog = xdp_program__create(&xdp_opts);
    int err = libxdp_get_error(prog);
    if (err) {
        char errmsg[1024];
        libxdp_strerror(err, errmsg, sizeof(errmsg));
        lwlog_err("ERR: loading program: %s\n", errmsg);
        exit(EXIT_FAIL_BPF);
    }

    /* At this point: All XDP/BPF programs from the cfg->filename have been
     * loaded into the kernel, and evaluated by the verifier. Only one of
     * these gets attached to XDP hook, the others will get freed once this
     * process exit.
     */

    /* At this point: BPF-progs are (only) loaded by the kernel, and prog_fd
     * is our select file-descriptor handle. Next step is attaching this FD
     * to a kernel hook point, in this case XDP net_device link-level hook.
     */
    err = xdp_program__attach(prog, cfg->ifindex, cfg->attach_mode, 0);
    if (err)
        exit(err);

    const int prog_fd = xdp_program__fd(prog);
    if (prog_fd < 0) {
        lwlog_err("ERR: xdp_program__fd failed: %s\n", strerror(errno));
        exit(EXIT_FAIL_BPF);
    }

    return prog;
}

#define XDP_UNKNOWN XDP_REDIRECT + 1
#ifndef XDP_ACTION_MAX
#define XDP_ACTION_MAX (XDP_UNKNOWN + 1)
#endif

static const char* xdp_action_names[XDP_ACTION_MAX] = {
    [XDP_ABORTED] = "XDP_ABORTED", [XDP_DROP] = "XDP_DROP",         [XDP_PASS] = "XDP_PASS",
    [XDP_TX] = "XDP_TX",           [XDP_REDIRECT] = "XDP_REDIRECT", [XDP_UNKNOWN] = "XDP_UNKNOWN",
};

const char* action2str(__u32 action) {
    if (action < XDP_ACTION_MAX)
        return xdp_action_names[action];
    return NULL;
}

int check_map_fd_info(const struct bpf_map_info* info, const struct bpf_map_info* exp) {
    if (exp->key_size && exp->key_size != info->key_size) {
        fprintf(stderr,
                "ERR: %s() "
                "Map key size(%d) mismatch expected size(%d)\n",
                __func__, info->key_size, exp->key_size);
        return EXIT_FAIL;
    }
    if (exp->value_size && exp->value_size != info->value_size) {
        fprintf(stderr,
                "ERR: %s() "
                "Map value size(%d) mismatch expected size(%d)\n",
                __func__, info->value_size, exp->value_size);
        return EXIT_FAIL;
    }
    if (exp->max_entries && exp->max_entries != info->max_entries) {
        fprintf(stderr,
                "ERR: %s() "
                "Map max_entries(%d) mismatch expected size(%d)\n",
                __func__, info->max_entries, exp->max_entries);
        return EXIT_FAIL;
    }
    if (exp->type && exp->type != info->type) {
        fprintf(stderr,
                "ERR: %s() "
                "Map type(%d) mismatch expected type(%d)\n",
                __func__, info->type, exp->type);
        return EXIT_FAIL;
    }

    return 0;
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

/* Pinning maps under /sys/fs/bpf in subdir */
int pin_maps_in_bpf_object(struct bpf_object* bpf_obj, const char* subdir, char* map_name) {
    char map_filename[PATH_MAX];
    char pin_dir[PATH_MAX];
    int err;

    int len = snprintf(pin_dir, PATH_MAX, "%s/%s", pin_basedir, subdir);
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

int load_xdp_program(const struct config* cfg, struct xdp_program* prog, char* map_name) {
    DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts);
    DECLARE_LIBXDP_OPTS(xdp_program_opts, xdp_opts, 0);
    if (!cfg->filename) {
        lwlog_err("No filename specified");
        return -1;
    }

    if (!cfg->ifname) {
        lwlog_err("No interface name specified");
        return -1;
    }

    char errmsg[1024];
    xdp_opts.open_filename = cfg->filename;
    xdp_opts.prog_name = cfg->progname;
    xdp_opts.opts = &opts;
    if (!cfg->progname) {
        prog = xdp_program__create(&xdp_opts);
    } else {
        prog = xdp_program__open_file(cfg->filename, NULL, &opts);
    }

    int err = libxdp_get_error(prog);
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

    if (map_name == NULL) {
        lwlog_info("No map name specified for %s, not pinning maps", cfg->ifname);
        return 0;
    }

    /* Pin the maps */
    err = pin_maps_in_bpf_object(xdp_program__bpf_obj(prog), cfg->ifname, map_name);
    if (err) {
        lwlog_err("ERR: pinning maps in %s", cfg->ifname);
        return err;
    }

    return 0;
}

int do_unload(const struct config* cfg) {
    struct xdp_multiprog* mp = NULL;
    int err = EXIT_FAILURE;
    DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts);

    mp = xdp_multiprog__get_from_ifindex(cfg->ifindex);
    if (libxdp_get_error(mp)) {
        lwlog_warning("Unable to get xdp_multiprog program for ifname %s", cfg->ifname);
        goto defer;
    } else if (!mp) {
        lwlog_warning("No XDP program loaded on %s", cfg->ifname);
        mp = NULL;
        goto defer;
    }

    if (cfg->unload_all) {
        err = xdp_multiprog__detach(mp);
        if (err) {
            lwlog_warning("Unable to detach XDP program: %s", strerror(-err));
            goto defer;
        }
    } else {
        enum xdp_attach_mode mode;
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
        goto defer;

    found:
        lwlog_info("Detaching XDP program with ID %u from %s", xdp_program__id(prog), cfg->ifname);
        err = xdp_program__detach(prog, cfg->ifindex, mode, 0);
        if (err) {
            lwlog_warning("Unable to detach XDP program: %s", strerror(-err));
            goto defer;
        }
    }

defer:
    xdp_multiprog__close(mp);
    return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
