OBJ_PATH := obj
SRC_PATH := src
BIN_PATH := bin
INC_PATH := include

# SRC_FILES := $(wildcard $(SRC_PATH)/*.c)
KERN_SRC := $(SRC_PATH)/af_xdp.c $(SRC_PATH)/xdp_kern.c
# Exclude kernel sources from SRC_FILES
SRC_FILES := $(filter-out $(KERN_SRC), $(wildcard $(SRC_PATH)/*.c))

USER_SRC := $(filter-out $(SRC_PATH)/xdp_daemon.c, $(SRC_FILES))
DAEMON_SRC := $(filter-out $(SRC_PATH)/xdp_user.c, $(SRC_FILES))
HEADERS := $(wildcard $(INC_PATH)/*.h)


CXX := clang
CFLAGS := -O2 -g -Wall -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -I$(INC_PATH)# -fsanitize=address -fsanitize=undefined -fsanitize=bounds -fsanitize=nullability  -fsanitize=integer -fsanitize=object-size -fsanitize=shift -fsanitize=unreachable -fsanitize=vla-bound -fsanitize=vptr
CCOBJFLAGS := $(CFLAGS) -c
CXXBPFFLAGS := -O2 -g -Wall -target bpf -D __BPF_TRACING__ -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types 
CCOBJBPFFLAGS := $(CXXBPFFLAGS) -c


all: $(BIN_PATH)/xdp_daemon $(BIN_PATH)/xdp_user $(OBJ_PATH)/af_xdp.o $(OBJ_PATH)/xdp_kern.o $(OBJ_PATH)/xdp_dummy.o

$(OBJ_PATH)/xdp_dummy.o: $(SRC_PATH)/xdp_dummy.c
	$(CXX) $(CCOBJBPFFLAGS) -o $@ $<

$(OBJ_PATH)/af_xdp.o: $(SRC_PATH)/af_xdp.c 
	$(CXX) $(CCOBJBPFFLAGS) -o $@ $<

$(OBJ_PATH)/xdp_kern.o: $(SRC_PATH)/xdp_kern.c
	$(CXX) $(CCOBJBPFFLAGS) -o $@ $<

$(BIN_PATH)/xdp_daemon: $(DAEMON_SRC) $(HEADERS)
	$(CXX) $(CFLAGS) -o $@ $(DAEMON_SRC) -lxdp -lbpf

$(BIN_PATH)/xdp_user: $(USER_SRC) $(HEADERS)
	$(CXX) $(CFLAGS) -o $@ $(USER_SRC) -lxdp -lbpf

clean:
	rm -rf $(OBJ_PATH)/*.o $(BIN_PATH)/*

.PHONY: all clean