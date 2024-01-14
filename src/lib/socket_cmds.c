#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>

#include "lwlog.h"
#include "socket_handler.h"
#include "socket_cmds.h"
#include "veth_list.h"
#include "socket.h"

enum { CMD_SIZE = 1024 };

// creates veth pair with the given prefix i.e. "test" -> "test_inner" and "test_outer"
void create_port(const char* prefix) {
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
    const int err = system(cmd);
    if (err != 0) {
        lwlog_err("Failed to create veth pair: [%s, %s]", inner, outer);
    }
}

void delete_port(const char* prefix) {
    char inner[IFNAMSIZ];
    snprintf(inner, IFNAMSIZ, "%s_inner", prefix);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

    lwlog_info("Removing veth pair from list: [%s]", inner);
    if (veth_list_remove(veths, inner) < 0) {
        lwlog_err("Failed to remove veth pair: [%s]", inner);
    }

    char cmd[CMD_SIZE];
    snprintf(cmd, CMD_SIZE, "./scripts/delete_veth.sh %s", inner);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)

    lwlog_info("Deleting veth pair: [%s]", inner);
    const int err = system(cmd);
    if (err != 0) {
        lwlog_err("Failed to delete veth pair: [%s]", inner);
    }
}