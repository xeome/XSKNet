# XSKNet

## Introduction

This project implements Kernel bypass using AF_XDP (XSK) to implement a custom networking stack. It bypasses the traditional kernel stack and achieving superior throughput and low-latency communication.

*Note that this project is designed for a specific use case and is not intended to be a general purpose networking stack.*

## Project architecture

![Untitled-2023-11-18-2228_dark](https://github.com/xeome/af_xdp-l2fwd/assets/44901648/1fa453b2-414b-434c-afe8-5ce034ca71dd)

## Build

Ensure libxdp and libbpf are installed.

```sh
make
```

## Progress

- [x] Redirect packets from PHY to veth
- [x] Receive packets from veth in user space using AF_XDP
- [x] Transmit packets from user space to veth using AF_XDP
- [ ] Build a dynamic routing table for retransmission

## Usage

### Daemon

To run the daemon, you need to specify physical interface and the path to the XDP object file.

```sh
sudo bin/xdp_daemon --dev wlan0 --filename obj/xdp_kern.o
```

### User client

Pass `-p` to enable polling mode. -d specifies the name of the veth interface.

```sh
sudo bin/xdp_user -d test -p
```
