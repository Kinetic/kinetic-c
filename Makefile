PROJECT = kinetic-c-client
UTILITY = kinetic-c-util

OUT_DIR = ./obj
BIN_DIR = ./bin
PUB_INC = ./include
API_NAME = kinetic_client
LIB_DIR = ./src/lib
PBC_INC = ./vendor/protobuf-c
PBC_LIB = ./vendor/protobuf-c
SOCKET99 = ./vendor/socket99
VND_INC = ./vendor
BIN = $(BIN_DIR)/kinetic_client
LDFLAGS += -lm -l crypto -l ssl

PREFIX ?= /usr/local
INSTALL ?= install
RM ?= rm

VERSION_FILE = ./VERSION
VERSION = ${shell head -n1 $(VERSION_FILE)}
KINETIC_LIB_NAME = $(PROJECT).$(VERSION)
KINETIC_LIB = $(BIN_DIR)/lib$(KINETIC_LIB_NAME).a
KINETIC_SO_DEV = $(BIN_DIR)/lib$(KINETIC_LIB_NAME).so
KINETIC_SO_RELEASE = $(PREFIX)/lib$(KINETIC_LIB_NAME).so
CC = gcc
OPTIMIZE = -O3
WARN = -Wall -Wextra -pedantic
# This is necessary because the library depends on
# both C99 _and_ POSIX (for the BSD sockets API).
CDEFS += -D_POSIX_C_SOURCE=1 -D_C99_SOURCE=1
CFLAGS += -std=c99 -fPIC -g $(WARN) $(CDEFS) $(OPTIMIZE)

LIB_INCS = -I$(LIB_DIR) -I$(PUB_INC) -I$(PBC_LIB) -I$(PBC_INC) -I$(VND_INC)
LIB_DEPS = $(PUB_INC)/kinetic_client.h $(PUB_INC)/byte_array.h $(PUB_INC)/kinetic_types.h $(LIB_DIR)/kinetic_connection.h $(LIB_DIR)/kinetic_hmac.h $(LIB_DIR)/kinetic_logger.h $(LIB_DIR)/kinetic_message.h $(LIB_DIR)/kinetic_nbo.h $(LIB_DIR)/kinetic_operation.h $(LIB_DIR)/kinetic_pdu.h $(LIB_DIR)/kinetic_proto.h $(LIB_DIR)/kinetic_socket.h $(LIB_DIR)/kinetic_types_internal.h
# LIB_OBJ = $(patsubst %,$(OUT_DIR)/%,$(LIB_OBJS))
LIB_OBJS = $(OUT_DIR)/kinetic_allocator.o $(OUT_DIR)/kinetic_nbo.o $(OUT_DIR)/kinetic_operation.o $(OUT_DIR)/kinetic_pdu.o $(OUT_DIR)/kinetic_proto.o $(OUT_DIR)/kinetic_socket.o $(OUT_DIR)/kinetic_message.o $(OUT_DIR)/kinetic_logger.o $(OUT_DIR)/kinetic_hmac.o $(OUT_DIR)/kinetic_connection.o $(OUT_DIR)/kinetic_types.o $(OUT_DIR)/kinetic_types_internal.o $(OUT_DIR)/byte_array.o $(OUT_DIR)/kinetic_client.o $(OUT_DIR)/socket99.o $(OUT_DIR)/protobuf-c.o 

default: $(KINETIC_LIB)

test: Rakefile $(LIB_OBJS)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Testing $(PROJECT)
	@echo --------------------------------------------------------------------------------
	bundle install
	bundle exec rake ci

clean:
	rm -rf $(BIN_DIR)/* $(OUT_DIR)/*.o *.core

.PHONY: clean

# $(OUT_DIR)/%.o: %.c $(DEPS)
# 	$(CC) -c -o $@ $< $(CFLAGS)

$(OUT_DIR)/kinetic_allocator.o: $(LIB_DIR)/kinetic_allocator.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_nbo.o: $(LIB_DIR)/kinetic_nbo.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_pdu.o: $(LIB_DIR)/kinetic_pdu.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_proto.o: $(LIB_DIR)/kinetic_proto.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_socket.o: $(LIB_DIR)/kinetic_socket.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_message.o: $(LIB_DIR)/kinetic_message.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_logger.o: $(LIB_DIR)/kinetic_logger.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_hmac.o: $(LIB_DIR)/kinetic_hmac.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_connection.o: $(LIB_DIR)/kinetic_connection.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_operation.o: $(LIB_DIR)/kinetic_operation.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_types.o: $(LIB_DIR)/kinetic_types.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_types_internal.o: $(LIB_DIR)/kinetic_types_internal.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/byte_array.o: $(LIB_DIR)/byte_array.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/socket99.o: $(SOCKET99)/socket99.c $(SOCKET99)/socket99.h
	$(CC) -c -o $@ $< $(CFLAGS) -I$(SOCKET99)
$(OUT_DIR)/protobuf-c.o: $(PBC_LIB)/protobuf-c/protobuf-c.c $(PBC_LIB)/protobuf-c/protobuf-c.h
	$(CC) -c -o $@ $< -std=c99 -fPIC -g -Wall $(OPTIMIZE) -I$(PBC_LIB)
$(OUT_DIR)/kinetic_client.o: $(LIB_DIR)/kinetic_client.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)

$(KINETIC_LIB): $(LIB_OBJS) $(VERSION_FILE)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Building static library: $(KINETIC_LIB)
	@echo --------------------------------------------------------------------------------
	ar -rcs $@ $(LIB_OBJS)
	ar -t $@

# $(KINETIC_SO): $(KINETIC_LIB)
# 	@echo
# 	@echo --------------------------------------------------------------------------------
# 	@echo Building dynamic library: $(KINETIC_SO)
# 	@echo --------------------------------------------------------------------------------
# 	$(CC)(-) $(KINETIC_SO_DEV) -shared $(LDFLAGS) $(LIB_OBJS)

# libso: $(KINETIC_SO)

UTIL_DIR = ./src/utility
UTIL_EX = $(UTIL_DIR)/examples
UTIL_EXEC_NAME = $(UTILITY)
UTIL_INTERNAL_OBJS = $(OUT_DIR)/noop.o $(OUT_DIR)/put.o $(OUT_DIR)/get.o $(OUT_DIR)/delete.o
UTIL_OBJS = $(OUT_DIR)/main.o $(UTIL_INTERNAL_OBJS)
DEV_UTIL_INCS = -I./include -I$(UTIL_DIR)
DEV_UTIL_LDFLAGS += $(LDFLAGS) $(KINETIC_LIB)

# $(OUT_DIR)/noop.o: $(UTIL_EX)/noop.c $(UTIL_EX)/noop.h
# 	$(CC) -c -o $@ $< $(CFLAGS) $(DEV_UTIL_INCS)
# $(OUT_DIR)/put.o: $(UTIL_EX)/put.c $(UTIL_EX)/put.h
# 	$(CC) -c -o $@ $< $(CFLAGS) $(DEV_UTIL_INCS)
# $(OUT_DIR)/get.o: $(UTIL_EX)/get.c $(UTIL_EX)/get.h
# 	$(CC) -c -o $@ $< $(CFLAGS) $(DEV_UTIL_INCS)
# $(OUT_DIR)/delete.o: $(UTIL_EX)/delete.c $(UTIL_EX)/delete.h
# 	$(CC) -c -o $@ $< $(CFLAGS) $(DEV_UTIL_INCS)
# $(OUT_DIR)/main.o: $(UTIL_DIR)/main.c $(UTIL_INTERNAL_OBJS)
# 	$(CC) -c -o $@ $< $(CFLAGS) $(DEV_UTIL_INCS)

# UTIL_NAME = $(UTIL_EXEC_NAME)
# DEV_UTIL_EXEC = $(BIN_DIR)/$(DEV_UTIL_NAME)

# $(DEV_UTIL_EXEC): $(UTIL_OBJS) $(KINETIC_LIB)
# 	@echo
# 	@echo --------------------------------------------------------------------------------
# 	@echo Building development test utility: $(DEV_UTIL_EXEC)
# 	@echo --------------------------------------------------------------------------------
# 	$(CC) -o $@ $(UTIL_OBJS) $(CFLAGS) $(DEV_UTIL_LDFLAGS)


# # Common release includes and linker flags
# REL_UTIL_INCS = -I$(PREFIX)/include -I$(UTIL_DIR)
# REL_UTIL_LDFLAGS += $(LDFLAGS) -l $(KINETIC_LIB)

# REL_UTIL_EXEC_STATIC_NAME = $(UTIL_EXEC_NAME)-rel-static
# REL_UTIL_EXEC_STATIC = $(BIN_DIR)/$(REL_UTIL_EXEC_STATIC_NAME)

# $(REL_UTIL_EXEC_STATIC): $(UTIL_OBJS)
# 	@echo
# 	@echo --------------------------------------------------------------------------------
# 	@echo Building release $(UTIL_EXEC) against installed static library
# 	@echo --------------------------------------------------------------------------------
# 	$(CC) -o $@ $(UTIL_OBJS) $(REL_UTIL_INCS) $(CFLAGS) $(LDFLAGS) 


# REL_UTIL_EXEC_NAME = $(UTIL_EXEC_NAME)
# REL_UTIL_EXEC = $(BIN_DIR)/$(REL_UTIL_EXEC_NAME)

# $(REL_UTIL_EXEC): $(UTIL_DIR)/main.c $(UTIL_OBJS)
# 	@echo
# 	@echo --------------------------------------------------------------------------------
# 	@echo Building release $(UTIL_EXEC) against installed dynamic library
# 	@echo --------------------------------------------------------------------------------
# 	$(CC) -o $@ -L$(PREFIX) -l $(PROJECT).so.$(VERSION) $< $(UTIL_OBJS) $(REL_UTIL_INCS) $(CFLAGS) $(LDFLAGS)

# utility: $(UTIL_EXEC) $(REL_UTIL_DYN)
# utility: $(UTIL_EXEC)

# Configure to launch java simulator
# JAVA=$(JAVA_HOME)/bin/java
SIM_JARS_PREFIX = vendor/kinetic-java/kinetic-simulator-0.7.0.2-kinetic-proto-2.0.6-SNAPSHOT
CLASSPATH = $(JAVA_HOME)/lib/tools.jar:$(SIM_JARS_PREFIX)-jar-with-dependencies.jar:$(SIM_JARS_PREFIX)-sources.jar:$(SIM_JARS_PREFIX).jar
SIM_RUNNER = com.seagate.kinetic.simulator.internal.SimulatorRunner
SIM_ADMIN = com.seagate.kinetic.admin.cli.KineticAdminCLI

# run: $(DEV_UTIL_EXEC)
# 	@echo
# 	@echo --------------------------------------------------------------------------------
# 	@echo Running test utility: $(DEV_UTIL_EXEC)
# 	@echo --------------------------------------------------------------------------------
# 	@sleep 2
# 	exec java -classpath "$(CLASSPATH)" $(SIM_RUNNER) "$@" &
# 	@sleep 5
# 	exec java -classpath "$(CLASSPATH)" $(SIM_ADMIN) -setup -erase true
# 	$(DEV_UTIL_EXEC) noop
# 	$(DEV_UTIL_EXEC) put
# 	$(DEV_UTIL_EXEC) get
# 	$(DEV_UTIL_EXEC) delete
# 	exec pkill -f 'java.*kinetic-simulator'

# rund: $(REL_UTIL_DYN)
# 	@echo
# 	@echo --------------------------------------------------------------------------------
# 	@echo Running $(UTIL_EXEC) $(PROJECT) test utility \(dynamically linked\)
# 	@echo --------------------------------------------------------------------------------
# 	@sleep 2
# 	exec java -classpath "$(CLASSPATH)" $(SIM_RUNNER) "$@" &
# 	@sleep 5
# 	exec java -classpath "$(CLASSPATH)" $(SIM_ADMIN) -setup -erase true
# 	$(REL_UTIL_DYN) noop
# 	$(REL_UTIL_DYN) put
# 	$(REL_UTIL_DYN) get
# 	$(REL_UTIL_DYN) delete
# 	exec pkill -f 'java.*kinetic-simulator'

# Installation
KINETIC_LIB_NAME = $(PROJECT).$(VERSION)
KINETIC_LIB = $(BIN_DIR)/lib$(KINETIC_LIB_NAME).a
KINETIC_SO_DEV = $(BIN_DIR)/lib$(KINETIC_LIB_NAME).so
KINETIC_SO_RELEASE = $(PREFIX)/lib$(KINETIC_LIB_NAME).so
# install: $(KINETIC_LIB) $(KINETIC_SO_DEV) 
install: $(KINETIC_LIB) 
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Installing $(PROJECT) v$(VERSION) into $(PREFIX)
	@echo --------------------------------------------------------------------------------
	@echo
	@echo You may be prompted for your password in order to proceed.
	@echo
	$(INSTALL) -d $(PREFIX)/lib/
	$(INSTALL) -c $(KINETIC_LIB) $(PREFIX)/lib/
	# $(INSTALL) -c $(KINETIC_SO_DEV) $(PREFIX)/lib/
	$(INSTALL) -d $(PREFIX)/include/
	$(INSTALL) -c ./include/$(API_NAME).h $(PREFIX)/include/
	$(INSTALL) -c ./include/kinetic_types.h $(PREFIX)/include/

uninstall:
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Uninstalling $(PROJECT) from $(PREFIX)
	@echo --------------------------------------------------------------------------------
	@echo
	@echo You may be prompted for your password in order to proceed.
	@echo
	$(RM) -f $(PREFIX)/lib/lib$(PROJECT)*.a
	$(RM) -f $(PREFIX)/lib/lib$(PROJECT)*.so
	$(RM) -f $(PREFIX)/include/${PUBLIC_API}.h
	$(RM) -f $(PREFIX)/include/kinetic_types.h
	$(RM) -f $(PREFIX)/include/kinetic_proto.h
	$(RM) -f $(PREFIX)/include/protobuf-c/protobuf-c.h
	$(RM) -f $(PREFIX)/include/protobuf-c.h

# all: uninstall clean test default install run rund
# all: clean test default run
all: clean test default

# ci: uninstall clean test default install rund
# 	@echo
# 	@echo --------------------------------------------------------------------------------
# 	@echo $(PROJECT) build completed successfully!
# 	@echo --------------------------------------------------------------------------------
# 	@echo $(PROJECT) v$(VERSION) is in working order
# 	@echo

# Other dependencies
$(BIN_DIR)/lib${PROJECT}.a: Makefile Rakefile VERSION
# kinetic-lib.o: kinetic_client.h kinetic_connection.h kinetic_hmac.h kinetic_logger.h kinetic_message.h kinetic_nbo.h kinetic_operation.h kinetic_pdu.h kinetic_proto.h kinetic_socket.h protobuf-c.h socket99.h
