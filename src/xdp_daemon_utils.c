#include <stdlib.h>

#include "daemon_api.h"
#include "lwlog.h"

char** veth_list;

int init_veth_list() {
    veth_list = malloc(sizeof(char*) * 100);
    for (int i = 0; i < 100; i++) {
        veth_list[i] = NULL;
    }
    return 0;
}

int add_to_veth_list(char* veth_name) {
    int i = 0;
    while (veth_list[i] != NULL) {
        i++;
    }
    veth_list[i] = veth_name;
    return i;
}

int remove_from_veth_list(char* veth_name) {
    int i = 0;
    while (veth_list[i] != NULL) {
        if (strcmp(veth_list[i], veth_name) == 0) {
            veth_list[i] = NULL;
            return i;
        }
        i++;
    }
    return -1;
}

bool create_veth(char* veth_name) {
    char cmd[1024];
    sprintf(cmd, "./testenv/testenv.sh setup --name=%s", veth_name);
    int err = system(cmd);
    if (err) {
        lwlog_err("Couldn't create veth pair: (%d)", err);
        return false;
    }
    return true;
}

bool delete_veth(char* veth_name) {
    char cmd[1024];
    sprintf(cmd, "./testenv/testenv.sh teardown --name=%s", veth_name);
    int err = system(cmd);
    if (err) {
        lwlog_err("Couldn't delete veth pair: (%d)", err);
        return false;
    }
    return true;
}

char** get_veth_list() {
    return veth_list;
}