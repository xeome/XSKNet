CXX := clang
CXXFLAGS := -O2 -g -Wall -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types 
CCOBJFLAGS := $(CXXFLAGS) -c
CXXBPFFLAGS := -O2 -g -Wall -target bpf -D __BPF_TRACING__ -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -Werror 
CCOBJBPFFLAGS := $(CXXBPFFLAGS) -c

OBJ_PATH := obj
SRC_PATH := src
BIN_PATH := bin
INC_PATH := include

SRC_FILES := $(wildcard $(SRC_PATH)/*.c)
KERN_SRC := $(SRC_PATH)/xdp_kern.c

all: $(BIN_PATH)/xdp_daemon $(OBJ_PATH)/xdp_kern_obj.o

$(OBJ_PATH)/xdp_kern_obj.o: $(KERN_SRC)
	$(CXX) $(CCOBJBPFFLAGS) -I$(INC_PATH) -o $@ $(KERN_SRC)

$(BIN_PATH)/xdp_daemon: $(SRC_FILES)
	$(CXX) $(CXXFLAGS) -I$(INC_PATH) -o $@ $^ -lxdp -lbpf


clean:
	rm -rf $(OBJ_PATH)/*.o $(BIN_PATH)/*

.PHONY: all clean