OUT_DIR = obj
BIN_DIR = bin
OBJS = $(OUT_DIR)/protobuf-c.o $(OUT_DIR)/kinetic_proto.o $(OUT_DIR)/kinetic_client.o
INCS = -I"./include" -I"./src/lib" -I"./vendor/protobuf=c" -I"./vendor"
BIN = $(BIN_DIR)/kinetic_client
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

$(OUT_DIR)/protobuf-c.o: directories ./vendor/protobuf-c/protobuf-c.c
	$(CC) $(CFLAGS) -c ./src/vendor/protobuf-c/protobuf-c.c -o $@

$(OUT_DIR)/kinetic_proto.o: directories /src/lib/kinetic_proto.c ./vendor/protobuf-c/protobuf-c.c
	$(CC) $(CFLAGS) -c ./src/lib/kinetic_proto.c -o $@

$(OUT_DIR)/kinetic_client.o: directories ./src/lib/kinetic_client.c /src/lib/kinetic_proto.c ./vendor/protobuf-c/protobuf-c.c
	$(CC) $(CFLAGS) -c ./src/lib/kinetic_client.c -o $@

$(BIN): $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o $(BIN)

run:
	@echo=off; echo "\nRunning Executable $(BIN):"
	./$(BIN)
