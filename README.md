# AF_XDP L2 Forwarding

## Introduction

This project implements Kernel bypass using AF_XDP and achieves high performance packet forwarding to user space.

## Project architecture

![Untitled-2023-11-18-2228_dark](https://github.com/xeome/af_xdp-l2fwd/assets/44901648/1fa453b2-414b-434c-afe8-5ce034ca71dd)

## Build

You need libxdp and libbpf installed.

```sh
make
```

## Progress

- [x] Redirect packets from PHY to veth
- [x] Receive packets from veth in user space using AF_XDP
- [x] Transmit packets from user space to veth using AF_XDP
- [ ] Build routing table for retransmission

## Usage

### Daemon

```sh
sudo bin/xdp_daemon -d test --filename obj/xdp_kern_obj.o
```

### User client

```sh
sudo bin/xdp_user -d test -p
```
