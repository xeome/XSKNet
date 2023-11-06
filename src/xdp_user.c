#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <bpf/bpf.h>
#include <xdp/libxdp.h>
#include <xdp/xsk.h>

#include "xdp_socket.h"
#include "xdp_receive.h"
#include "defs.h"
#include "lwlog.h"

int main(int argc, char** argv) {}