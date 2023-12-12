#include <stdlib.h>

#include "libxsk.h"
#include "lwlog.h"

char** veth_list;

int init_veth_list() {
    veth_list = calloc(VETH_NUM, sizeof(char*));
    if (!veth_list) {
        lwlog_err("Failed to allocate memory for veth_list");
        exit(EXIT_FAILURE);
    }
    return 0;
}

int add_to_veth_list(const char* veth_name) {
    if (!veth_name) {
        lwlog_err("veth_name is NULL");
        return -1;
    }

    for (int i = 0; i < VETH_NUM; i++) {
        if (!veth_list[i]) {
            veth_list[i] = strdup(veth_name);
            lwlog_info("Added %s to veth list", veth_list[i]);
            return 0;
        }
    }
    return -1;  // List is full
}

int remove_from_veth_list(const char* veth_name) {
    if (!veth_name) {
        lwlog_err("veth_name is NULL");
        return -1;
    }

    for (int i = 0; i < VETH_NUM; i++) {
        if (veth_list[i] != NULL && strcmp(veth_list[i], veth_name) == 0) {
            free(veth_list[i]);
            veth_list[i] = NULL;
            return 0;
        }
    }
    return -1;  // Name not found
}

void create_veth(const char* veth_name) {
    if (!veth_name) {
        lwlog_err("veth_name is NULL");
        return;
    }

    char cmd[1024];
    sprintf(cmd, "./testenv/create_veth.sh %s %s_peer 10.0.0.%d 10.0.0.%d", veth_name, veth_name, 2, 1);
    lwlog_info("Running command: %s", cmd);
    int err = system(cmd);
    if (err) {
        lwlog_err("Couldn't create veth pair: (%d)", err);
    }
}

void delete_veth(const char* veth_name) {
    if (!veth_name) {
        lwlog_err("veth_name is NULL");
        return;
    }

    errno = 0;
    if (strcmp(veth_name, "lo") == 0) {
        lwlog_err("Can't delete loopback interface");
        return;
    }

    if (strcmp(veth_name, phy_ifname) == 0) {
        lwlog_info("Physical interface %s requested for deletion, ignoring", phy_ifname);
        return;
    }

    char cmd[1024];
    sprintf(cmd, "./testenv/delete_veth.sh %s", veth_name);
    int err = system(cmd);
    if (err) {
        lwlog_err("Couldn't delete veth pair: (%d)", err);
    }
}

char** get_veth_list() {
    return veth_list;
}