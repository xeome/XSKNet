#include <stdlib.h>
#include <net/if.h>

#include "libxsk.h"
#include "lwlog.h"

struct veth_pair** veth_list;

int init_veth_list() {
    veth_list = calloc(VETH_NUM, sizeof(struct veth_pair*));
    if (!veth_list) {
        lwlog_err("Failed to allocate memory for veth_list");
        exit(EXIT_FAILURE);
    }
    return 0;
}

int add_to_veth_list(const char* prefix) {
    if (!prefix) {
        lwlog_err("prefix is NULL");
        return -1;
    }

    for (int i = 0; i < VETH_NUM; i++) {
        if (!veth_list[i]) {
            veth_list[i] = calloc(1, sizeof(struct veth_pair));
            veth_list[i]->veth_outer = strdup(prefix);
            veth_list[i]->veth_inner = calloc(1, IFNAMSIZ);
            snprintf(veth_list[i]->veth_inner, IFNAMSIZ, "%s%s", veth_list[i]->veth_outer, "_peer");
            return i;
        }
    }
    return -1;  // List is full
}

int remove_from_veth_list(const char* veth_outer) {
    if (!veth_outer) {
        lwlog_err("veth_outer is NULL");
        return -1;
    }

    for (int i = 0; i < VETH_NUM; i++) {
        if (veth_list[i] != NULL && strcmp(veth_list[i]->veth_outer, veth_outer) == 0) {
            free(veth_list[i]->veth_outer);
            free(veth_list[i]->veth_inner);
            free(veth_list[i]);
            veth_list[i] = NULL;
            return i;
        }
    }
    return -1;  // Name not found
}

void create_veth(const char* veth_outer, const char* veth_inner) {
    if (!veth_outer) {
        lwlog_err("veth_outer is NULL");
        return;
    }

    char cmd[1024];
    sprintf(cmd, "./testenv/create_veth.sh %s %s", veth_outer, veth_inner);
    lwlog_info("Running command: %s", cmd);
    int err = system(cmd);
    if (err) {
        lwlog_err("Couldn't create veth pair: (%d)", err);
    }
}

void delete_veth(const char* veth_outer) {
    if (!veth_outer) {
        lwlog_err("veth_outer is NULL");
        return;
    }

    errno = 0;
    if (strcmp(veth_outer, "lo") == 0) {
        lwlog_err("Can't delete loopback interface");
        return;
    }

    char cmd[1024];
    sprintf(cmd, "./testenv/delete_veth.sh %s", veth_outer);
    int err = system(cmd);
    if (err) {
        lwlog_err("Couldn't delete veth pair: (%d)", err);
    }
}

struct veth_pair** get_veth_list() {
    return veth_list;
}

struct veth_pair* get_index(int index) {
    return veth_list[index];
}

struct veth_pair* get_pair(const char* prefix) {
    if (!prefix) {
        lwlog_err("prefix is NULL");
        return NULL;
    }

    for (int i = 0; i < VETH_NUM; i++) {
        if (veth_list[i] != NULL && strcmp(veth_list[i]->veth_outer, prefix) == 0) {
            return veth_list[i];
        }
    }
    return NULL;
}