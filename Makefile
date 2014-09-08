# OUT_DIR = obj
# BIN_DIR = bin
# OBJS = $(OUT_DIR)/protobuf-c.o $(OUT_DIR)/kinetic_proto.o $(OUT_DIR)/kinetic_client.o
# INCS = -I"./include" -I"./src/lib" -I"./vendor/protobuf=c" -I"./vendor"
# BIN = $(BIN_DIR)/kinetic_client
# CC = gcc
# CFLAGS = $(INCS) -g -Wall
# LFLAGS = -Wall
# MKDIRS = mkdir -p
# RM_ALL = rm -rf

# .PHONY: directories run

# build: directories $(BIN)

# all: clean build run

# directories: $(OUT_DIR) $(BIN_DIR)

# $(OUT_DIR):
# 	$(MKDIRS) $(OUT_DIR)

# $(BIN_DIR):
# 	$(MKDIRS) $(BIN_DIR)

# clean:
# 	$(RM_ALL) $(OBJS) $(OUT_DIR) $(BIN)

# $(OUT_DIR)/protobuf-c.o: directories ./vendor/protobuf-c/protobuf-c.c
# 	$(CC) $(CFLAGS) -c ./src/vendor/protobuf-c/protobuf-c.c -o $@

# $(OUT_DIR)/kinetic_proto.o: directories /src/lib/kinetic_proto.c ./vendor/protobuf-c/protobuf-c.c
# 	$(CC) $(CFLAGS) -c ./src/lib/kinetic_proto.c -o $@

# $(OUT_DIR)/kinetic_client.o: directories ./src/lib/kinetic_client.c /src/lib/kinetic_proto.c ./vendor/protobuf-c/protobuf-c.c
# 	$(CC) $(CFLAGS) -c ./src/lib/kinetic_client.c -o $@

# $(BIN): $(OBJS)
# 	$(CC) $(LFLAGS) $(OBJS) -o $(BIN)


LIB_NAME = kinetic_client
CC      = gcc
OUT_DIR = ./build/lib/obj
BIN_DIR = ./build/lib/bin
PUB_INC = ./include
PUB_API = $(PUB_INC)/$(LIB_NAME).h
LIB_INC = ./src/lib
EX_INC  = ./src/utility
PBC_INC = ./vendor/protobuf-c
SOCKET99 = ./vendor/socket99
VND_INC = ./vendor
INCS    = -I$(PUB_INC) -I$(LIB_INC) -I$(EX_INC) -I$(PBC_INC) -I$(VND_INC) -I$(SOCKET99)
CFLAGS  = $(INCS) -g -Wall
LIBS    = -lm -l crypto
BIN     = $(BIN_DIR)/kinetic_client

_LIB_DEPS = kinetic_connection.h kinetic_hmac.h kinetic_logger.h kinetic_message.h kinetic_nbo.h kinetic_operation.h kinetic_pdu.h kinetic_proto.h kinetic_socket.h kinetic_types.h
DEPS = $(PBC_INC)/protobuf-c.h $(SOCKET99)/socket99.h $(patsubst %,$(LIB_INC)/%,$(_LIB_DEPS))

LIB_OBJS = $(LIB_NAME).o kinetic_connection.o kinetic_hmac.o kinetic_logger.o kinetic_message.o kinetic_nbo.o kinetic_operation.o kinetic_pdu.o kinetic_proto.o kinetic_socket.o protobuf-c.o socket99.o
OBJ = $(patsubst %,$(OUT_DIR)/%,$(LIB_OBJS))

$(OUT_DIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OUT_DIR)/$(LIB_NAME).o: $(LIB_INC)/$(LIB_NAME).c
	$(CC) -c -o $@ $(INCS) $(CFLAGS)

$(LIB_NAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(OUT_DIR)/*.o *~ core

run:
	@echo=off; echo "\nRunning Executable $(BIN):"
	./$(BIN)


PROJECT = kinetic-c-client
OPTIMIZE = -O3
WARN = -Wall -Wextra -pedantic
#CDEFS += 
CFLAGS += -std=c99 -g ${WARN} ${CDEFS} ${OPTIMIZE}
#LDFLAGS +=

# A tautological compare is expected in the test suite.
# CFLAGS += -Wno-tautological-compare

all: lib${PROJECT}.a
# all: test_${PROJECT}

LIB_OBJS = $(LIB_NAME).o kinetic_connection.o kinetic_hmac.o kinetic_logger.o kinetic_message.o kinetic_nbo.o kinetic_operation.o kinetic_pdu.o kinetic_proto.o kinetic_socket.o protobuf-c.o socket99.o

# TEST_OBJS=

${PROJECT}: main.c ${OBJS}
	${CC} -o $@ main.c ${OBJS} ${LDFLAGS}

lib${PROJECT}.a: ${OBJS}
	ar -rcs lib${PROJECT}.a ${OBJS}

# test_${PROJECT}: test_${PROJECT}.c ${OBJS} ${TEST_OBJS}
# 	${CC} -o $@ test_${PROJECT}.c ${OBJS} ${TEST_OBJS} ${CFLAGS} ${LDFLAGS}

# test: ./test_${PROJECT}
# 	./test_${PROJECT}

clean:
	rm -f ${PROJECT} test_${PROJECT} *.o *.a *.core


# Installation
PREFIX ?=/usr/local
INSTALL ?= install
RM ?=rm

install: lib${PROJECT}.a
	${INSTALL} -d ${PREFIX}/lib/
	${INSTALL} -c lib${PROJECT}.a ${PREFIX}/lib/lib${PROJECT}.a
	${INSTALL} -d ${PREFIX}/include/
	${INSTALL} -c ${PROJECT}.h ${PREFIX}/include/
	${INSTALL} -c ${PROJECT}_types.h ${PREFIX}/include/

uninstall:
	${RM} -f ${PREFIX}/lib/lib${PROJECT}.a
	${RM} -f ${PREFIX}/include/${PROJECT}.h
	${RM} -f ${PREFIX}/include/${PROJECT}_types.h


# Other dependencies
theft.o: Makefile
theft.o: theft.h theft_types.h theft_types_internal.h



