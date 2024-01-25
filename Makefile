SRC_PATH := src
OBJ_PATH := obj
BIN_PATH := bin
INC_PATH := src/lib
LIB_PATH := src/lib
XDP_SRC_PATH := src/kern

CC := clang

SANITIZE := -fsanitize=address -fsanitize=undefined -fsanitize=bounds -fsanitize=nullability -fsanitize=integer -fsanitize=shift -fsanitize=unreachable -fsanitize=vla-bound -fsanitize=vptr
CFLAGS := -Wall -Wextra -I$(INC_PATH) -g -lxdp -lbpf#$(SANITIZE)
XDP_FLAGS := -O2 -g -Wall -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -target bpf -D __BPF_TRACING__ -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -c
DAEMON := $(BIN_PATH)/daemon
CLIENT := $(BIN_PATH)/client

SRC := $(wildcard $(SRC_PATH)/*.c)
LIB_SRC := $(wildcard $(LIB_PATH)/*.c)
XDP_SRC := $(wildcard $(XDP_SRC_PATH)/*.c)

all: $(DAEMON) $(CLIENT) $(OBJ_PATH)/phy_xdp.o $(OBJ_PATH)/inner_xdp.o $(OBJ_PATH)/outer_xdp.o

$(DAEMON): $(SRC_PATH)/daemon.c $(LIB_SRC) $(wildcard $(INC_PATH)/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC_PATH)/daemon.c $(LIB_SRC)

$(CLIENT): $(SRC_PATH)/client.c $(LIB_SRC) $(wildcard $(INC_PATH)/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC_PATH)/client.c $(LIB_SRC)

$(OBJ_PATH)/phy_xdp.o: $(XDP_SRC_PATH)/phy_xdp.c
	$(CC) $(XDP_FLAGS) -o $@ $<

$(OBJ_PATH)/inner_xdp.o: $(XDP_SRC_PATH)/inner_xdp.c
	$(CC) $(XDP_FLAGS) -o $@ $<

$(OBJ_PATH)/outer_xdp.o: $(XDP_SRC_PATH)/outer_xdp.c
	$(CC) $(XDP_FLAGS) -o $@ $<

clean:
	rm -rf $(OBJ_PATH)/*.o $(BIN_PATH)/*

.PHONY: all clean