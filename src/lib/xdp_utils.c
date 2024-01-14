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
        lwlog_info("Unpinning (remove) prev maps in %s/", pin_dir);

        /* Basically calls unlink(3) on map_filename */
        err = bpf_object__unpin_maps(bpf_obj, pin_dir);
        if (err) {
            lwlog_err("ERR: UNpinning maps in %s", pin_dir);
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
