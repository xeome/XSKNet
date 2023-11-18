#!/bin/bash

# This script creates a virtual ethernet which will be used by the test environment
veth1=$1
veth2=$2

function create_veth {
    ip link add ${veth1} type veth peer name ${veth2}
    ip link set up dev ${veth1}
    ip link set up dev ${veth2}
}

function turn_offload_off {
    ethtool --offload ${veth1} rx off tx off
    ethtool --offload ${veth2} rx off tx off
}

create_veth
turn_offload_off