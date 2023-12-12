#!/bin/bash

# This script creates a virtual ethernet which will be used by the test environment
veth1=$1
veth2=$2
veth1_ip=$3
veth2_ip=$4

iface_macaddr()
{
    local iface="$1"
    local ns="${2:-}"
    local output

    if [ -n "$ns" ]; then
        output=$(ip -br -n "$ns" link show dev "$iface")
    else
        output=$(ip -br link show dev "$iface")
    fi
    echo "$output" | awk '{print $3}'
}

function create_veth {
	ip link add "${veth1}" type veth peer name "${veth2}" >/dev/null
	ip link set up dev "${veth1}" >/dev/null
	ip link set up dev "${veth2}" >/dev/null
	ip addr add "${veth1_ip}" dev "${veth1}" >/dev/null
	ip addr add "${veth2_ip}" dev "${veth2}" >/dev/null
	ip neigh add "${veth2_ip}" lladdr "$(iface_macaddr "${veth2}")" dev "${veth1}" >/dev/null
	ip neigh add "${veth1_ip}" lladdr "$(iface_macaddr "${veth1}")" dev "${veth2}" >/dev/null
	ip route add "${veth2_ip}" dev "${veth1}" >/dev/null
	ip route add "${veth1_ip}" dev "${veth2}" >/dev/null
}

function turn_offload_off {
	ethtool --offload "${veth1}" rx off tx off >/dev/null
	ethtool --offload "${veth2}" rx off tx off >/dev/null
}

create_veth
turn_offload_off
