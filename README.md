# AF_XDP forwarding

## Introduction

This is a simple example of AF_XDP forwarding.

## Build

You need libxdp and libbpf installed.

```sh
make
```

## Usage

### Setup test environment

```sh
sudo ./testenv/testenv.sh setup --name=test
```

### Run

```sh
sudo bin/xdp_daemon -d test --filename obj/xdp_kern_obj.o -p
```
