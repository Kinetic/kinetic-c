OUT_DIR = obj
BIN_DIR = bin
OBJS = $(OUT_DIR)/protobuf-c.o $(OUT_DIR)/KineticProto.o $(OUT_DIR)/kinetic-c-client.o
INCS = -I"./include" -I"./src/main" -I"./src/vendor"
BIN = $(BIN_DIR)/kinetic-c-client
CC = gcc
CFLAGS = $(INCS) -g -Wall
LFLAGS = -Wall
MKDIRS = mkdir -p
RM_ALL = rm -rf

.PHONY: directories run

build: directories $(BIN)

all: clean build run

directories: $(OUT_DIR) $(BIN_DIR)

$(OUT_DIR):
	$(MKDIRS) $(OUT_DIR)

$(BIN_DIR):
	$(MKDIRS) $(BIN_DIR)

clean:
	$(RM_ALL) $(OBJS) $(OUT_DIR) $(BIN)

$(OUT_DIR)/protobuf-c.o: directories ./src/vendor/protobuf-c/protobuf-c.h ./src/vendor/protobuf-c/protobuf-c.c
	$(CC) $(CFLAGS) -c ./src/vendor/protobuf-c/protobuf-c.c -o $@

$(OUT_DIR)/KineticProto.o: directories ./src/main/KineticProto.h ./src/main/KineticProto.c ./src/vendor/protobuf-c/protobuf-c.h ./src/vendor/protobuf-c/protobuf-c.c
	$(CC) $(CFLAGS) -c ./src/main/KineticProto.c -o $@

$(OUT_DIR)/kinetic-c-client.o: directories ./src/main/main.c ./src/main/KineticProto.h ./src/main/KineticProto.c ./src/vendor/protobuf-c/protobuf-c.h ./src/vendor/protobuf-c/protobuf-c.c
	$(CC) $(CFLAGS) -c ./src/main/main.c -o $@

$(BIN): $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o $(BIN)

run:
	@echo=off; echo "\nRunning Executable $(BIN):"
	./$(BIN)
