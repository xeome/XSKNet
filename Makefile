OBJ_PATH := obj
SRC_PATH := src
BIN_PATH := bin
INC_PATH := include

SRC_FILES := $(wildcard $(SRC_PATH)/*.c)
KERN_SRC := $(SRC_PATH)/xdp_kern.c
USER_SRC := $(filter-out $(SRC_PATH)/xdp_daemon.c, $(SRC_FILES))
DAEMON_SRC := $(filter-out $(SRC_PATH)/xdp_user.c, $(SRC_FILES))
HEADERS := $(wildcard $(INC_PATH)/*.h)


CXX := clang
CFLAGS := -O2 -g -Wall -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -I$(INC_PATH) #-fsanitize=address -fsanitize=undefined -fsanitize=bounds -fsanitize=nullability  -fsanitize=integer -fsanitize=object-size -fsanitize=shift -fsanitize=unreachable -fsanitize=vla-bound -fsanitize=vptr
CCOBJFLAGS := $(CFLAGS) -c
CXXBPFFLAGS := -O2 -g -Wall -target bpf -D __BPF_TRACING__ -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types -Werror 
CCOBJBPFFLAGS := $(CXXBPFFLAGS) -c

all: $(BIN_PATH)/xdp_daemon $(OBJ_PATH)/xdp_kern_obj.o $(BIN_PATH)/xdp_user

$(OBJ_PATH)/xdp_kern_obj.o: $(KERN_SRC) 
	$(CXX) $(CCOBJBPFFLAGS) -o $@ $(KERN_SRC)

$(BIN_PATH)/xdp_daemon: $(DAEMON_SRC) $(HEADERS)
	$(CXX) $(CFLAGS) -o $@ $(DAEMON_SRC) -lxdp -lbpf

$(BIN_PATH)/xdp_user: $(USER_SRC) $(HEADERS)
	$(CXX) $(CFLAGS) -o $@ $(USER_SRC) -lxdp -lbpf

clean:
	rm -rf $(OBJ_PATH)/*.o $(BIN_PATH)/*

.PHONY: all clean