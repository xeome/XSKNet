#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <stdbool.h>

#include "daemon_api.h"
#include "lwlog.h"

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