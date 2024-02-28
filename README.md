# XSKNet

## Introduction

This project implements Kernel bypass using AF_XDP (XSK) to implement a custom networking stack. It bypasses the traditional kernel stack and achieving superior throughput and low-latency communication. Currently only ICMP responses are implemented

*Note that this project is designed for educational purposes and is not intended to be a general purpose networking stack.*

## Project architecture

![XSKNet](https://github.com/xeome/XSKNet/assets/44901648/8122d687-f21c-4dfa-b8a7-51f57dca400f)

## Build

Ensure libxdp and libbpf are installed.

```sh
make
```

## Progress

- [x] Redirect packets from PHY to veth
- [x] Receive packets from veth in user space using AF_XDP
- [x] Transmit packets from user space to veth using AF_XDP

## Usage

### Daemon

To run the daemon, you need to specify physical interface and the path to the XDP object file.

```sh
sudo bin/daemon --dev wlan0
```

### User client

```sh
sudo bin/client -d test
```
