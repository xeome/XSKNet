OBJ_PATH := obj
SRC_PATH := src
BIN_PATH := bin
INC_PATH := include 
XDP_KERNEL_SRC_PATH := $(SRC_PATH)/kern

# Exclude main files from the list of source files
MAINS = $(SRC_PATH)/xdp_daemon.c $(SRC_PATH)/xdp_user.c
XDP_USER_SRC = $(filter-out $(MAINS), $(wildcard $(SRC_PATH)/*.c))
LIB_SRC = $(wildcard $(SRC_PATH)/libxsk/*.c)
HEADERS := $(wildcard $(INC_PATH)/*.h)

CC := clang
# CFLAGS is used for non-bpf program compilation
CFLAGS := -O2 -g -Wall -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -I$(INC_PATH) #-fsanitize=address -fsanitize=undefined -fsanitize=bounds -fsanitize=nullability  -fsanitize=integer -fsanitize=object-size -fsanitize=shift -fsanitize=unreachable -fsanitize=vla-bound -fsanitize=vptr
# CCOBJBPFFLAGS is used for bpf program compilation
CCOBJBPFFLAGS := $(CFLAGS) -target bpf -D __BPF_TRACING__ -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -c

all: $(BIN_PATH)/xdp_daemon $(BIN_PATH)/xdp_user $(OBJ_PATH)/af_xdp.o $(OBJ_PATH)/xdp_kern.o $(OBJ_PATH)/xdp_dummy.o

# Compile XDP Kernel programs
$(OBJ_PATH)/xdp_dummy.o: $(XDP_KERNEL_SRC_PATH)/xdp_dummy.c
	$(CC) $(CCOBJBPFFLAGS) -o $@ $<

$(OBJ_PATH)/af_xdp.o: $(XDP_KERNEL_SRC_PATH)/af_xdp.c
	$(CC) $(CCOBJBPFFLAGS) -o $@ $<

$(OBJ_PATH)/xdp_kern.o: $(XDP_KERNEL_SRC_PATH)/xdp_kern.c
	$(CC) $(CCOBJBPFFLAGS) -o $@ $<

# Compile XDP user programs
$(BIN_PATH)/xdp_daemon: $(XDP_USER_SRC) $(LIB_SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(XDP_USER_SRC) $(LIB_SRC) $(SRC_PATH)/xdp_daemon.c -lxdp -lbpf

$(BIN_PATH)/xdp_user: $(XDP_USER_SRC) $(LIB_SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(XDP_USER_SRC) $(LIB_SRC) $(SRC_PATH)/xdp_user.c -lxdp -lbpf

clean:
	rm -rf $(OBJ_PATH)/*.o $(BIN_PATH)/*

.PHONY: all clean
