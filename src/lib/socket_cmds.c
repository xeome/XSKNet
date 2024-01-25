#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>

#include "lwlog.h"
#include "socket_handler.h"
#include "socket_cmds.h"
#include "veth_list.h"
#include "socket.h"
#include "xdp_utils.h"
#include "args.h"

enum { CMD_SIZE = 1024 };

// creates veth pair with the given prefix i.e. "test" -> "test_inner" and "test_outer"
void create_port(char* prefix) {
    char inner[IFNAMSIZ];
    char outer[IFNAMSIZ];
    snprintf(inner, IFNAMSIZ, "%s_inner", prefix);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    snprintf(outer, IFNAMSIZ, "%s_outer", prefix);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    lwlog_info("Adding veth pair to list: [%s, %s]", inner, outer);
    if (veth_list_add(veths, inner, outer) < 0) {
        lwlog_err("Failed to add veth pair: [%s, %s]", inner, outer);
    }

    char cmd[CMD_SIZE];
    snprintf(cmd, CMD_SIZE, "./scripts/create_veth.sh %s %s", inner, outer);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

    lwlog_info("Creating veth pair: [%s, %s]", inner, outer);
    int err = system(cmd);
    if (err != 0) {
        lwlog_err("Failed to create veth pair: [%s, %s]", inner, outer);
    }

    err = load_xdp_and_attach_to_ifname(outer, "obj/outer_xdp.o", "xdp_redirect_dummy_prog", NULL);
    if (err != EXIT_OK) {
        lwlog_err("load_xdp_and_attach_to_ifname: %s", strerror(err));
    }

    err = load_xdp_and_attach_to_ifname(inner, "obj/inner_xdp.o", "xdp_sock_prog", "xsks_map");
    if (err != EXIT_OK) {
        lwlog_err("load_xdp_and_attach_to_ifname: %s", strerror(err));
    }

    lwlog_info("Redirecting traffic from %s to %s", opts.dev, outer);
    err = update_devmap(if_nametoindex(outer), outer);
    if (err != EXIT_OK) {
        lwlog_err("Failed updating devmap: %s", strerror(err));
    }
}

void delete_port(char* prefix) {
    char inner[IFNAMSIZ];
    char outer[IFNAMSIZ];
    snprintf(inner, IFNAMSIZ, "%s_inner", prefix);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    snprintf(outer, IFNAMSIZ, "%s_outer", prefix);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

    lwlog_info("Removing veth pair from list: [%s]", inner);
    if (veth_list_remove(veths, inner) < 0) {
        lwlog_err("Failed to remove veth pair: [%s]", inner);
    }

    int err = unload_xdp_from_ifname(outer);
    if (err != EXIT_OK) {
        lwlog_err("unload_xdp_from_ifname: %s", strerror(err));
    }

    err = unload_xdp_from_ifname(inner);
    if (err != EXIT_OK) {
        lwlog_err("unload_xdp_from_ifname: %s", strerror(err));
    }

    char cmd[CMD_SIZE];
    snprintf(cmd, CMD_SIZE, "./scripts/delete_veth.sh %s", inner);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

    lwlog_info("Deleting veth pair: [%s, %s]", inner, outer);
    err = system(cmd);
    if (err != 0) {
        lwlog_err("Failed to delete veth pair: [%s, %s]", inner, outer);
    }
}

void unload_list() {
    for (int i = 0; i < veths->size; i++) {
        char prefix[IFNAMSIZ];
        // remove trailing _inner
        strncpy(prefix, veths->veth_pairs[i].veth1,
                strlen(veths->veth_pairs[i].veth1) - 6);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        delete_port(prefix);
    }
}