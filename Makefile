SRC_PATH := src
BIN_PATH := bin
INC_PATH := src/lib
LIB_PATH := src/lib

CC := clang
CFLAGS := -Wall -Wextra -I$(INC_PATH) -fsanitize=address -fsanitize=undefined -fsanitize=bounds -fsanitize=nullability  -fsanitize=integer -fsanitize=object-size -fsanitize=shift -fsanitize=unreachable -fsanitize=vla-bound -fsanitize=vptr
DAEMON := $(BIN_PATH)/daemon
CLIENT := $(BIN_PATH)/client

SRC := $(wildcard $(SRC_PATH)/*.c)
LIB_SRC := $(wildcard $(LIB_PATH)/*.c)

all: $(DAEMON) $(CLIENT)

$(DAEMON): $(SRC_PATH)/daemon.c $(LIB_SRC) $(wildcard $(INC_PATH)/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC_PATH)/daemon.c $(LIB_SRC)

$(CLIENT): $(SRC_PATH)/client.c $(LIB_SRC) $(wildcard $(INC_PATH)/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC_PATH)/client.c $(LIB_SRC)

clean:
	rm -rf $(OBJ_PATH)/*.o $(BIN_PATH)/*

.PHONY: all clean