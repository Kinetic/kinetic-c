#===============================================================================
# Configuration of Shared Paths
#===============================================================================
OUT_DIR = ./obj
BIN_DIR = ./bin
PUB_INC = ./include

#===============================================================================
# Shared Build Variables
#===============================================================================
CC ?= gcc
OPTIMIZE = -O3
WARN = -Wall -Wextra -pedantic
CDEFS += -D_POSIX_C_SOURCE=1 -D_C99_SOURCE=1
CFLAGS += -std=c99 -fPIC -g $(WARN) $(CDEFS) $(OPTIMIZE)
LDFLAGS += -lm -l crypto -l ssl

#===============================================================================
# Kinetic-C Library Build Support
#===============================================================================

PROJECT = kinetic-c-client

LIB_DIR = ./src/lib
VENDOR = ./vendor
PROTOBUFC = $(VENDOR)/protobuf-c
SOCKET99 = $(VENDOR)/socket99
VERSION_FILE = ./VERSION
VERSION = ${shell head -n1 $(VERSION_FILE)}

KINETIC_LIB_NAME = $(PROJECT).$(VERSION)
KINETIC_LIB = $(BIN_DIR)/lib$(KINETIC_LIB_NAME).a
LIB_INCS = -I$(LIB_DIR) -I$(PUB_INC) -I$(PROTOBUFC) -I$(VENDOR)
LIB_DEPS = $(PUB_INC)/kinetic_client.h $(PUB_INC)/byte_array.h $(PUB_INC)/kinetic_types.h $(LIB_DIR)/kinetic_connection.h $(LIB_DIR)/kinetic_hmac.h $(LIB_DIR)/kinetic_logger.h $(LIB_DIR)/kinetic_message.h $(LIB_DIR)/kinetic_nbo.h $(LIB_DIR)/kinetic_operation.h $(LIB_DIR)/kinetic_pdu.h $(LIB_DIR)/kinetic_proto.h $(LIB_DIR)/kinetic_socket.h $(LIB_DIR)/kinetic_types_internal.h
# LIB_OBJ = $(patsubst %,$(OUT_DIR)/%,$(LIB_OBJS))
LIB_OBJS = $(OUT_DIR)/kinetic_allocator.o $(OUT_DIR)/kinetic_nbo.o $(OUT_DIR)/kinetic_operation.o $(OUT_DIR)/kinetic_pdu.o $(OUT_DIR)/kinetic_proto.o $(OUT_DIR)/kinetic_socket.o $(OUT_DIR)/kinetic_message.o $(OUT_DIR)/kinetic_logger.o $(OUT_DIR)/kinetic_hmac.o $(OUT_DIR)/kinetic_connection.o $(OUT_DIR)/kinetic_types.o $(OUT_DIR)/kinetic_types_internal.o $(OUT_DIR)/byte_array.o $(OUT_DIR)/kinetic_client.o $(OUT_DIR)/socket99.o $(OUT_DIR)/protobuf-c.o
KINETIC_LIB_OTHER_DEPS = Makefile Rakefile $(VERSION_FILE)

default: $(KINETIC_LIB)

all: clean test default run

ci: uninstall all install
	@echo
	@echo --------------------------------------------------------------------------------
	@echo $(PROJECT) build completed successfully!
	@echo --------------------------------------------------------------------------------
	@echo $(PROJECT) v$(VERSION) is in working order
	@echo

test: Rakefile $(LIB_OBJS)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Testing $(PROJECT)
	@echo --------------------------------------------------------------------------------
	bundle install
	bundle exec rake test_all

clean:
	bundle exec rake clobber
	rm -rf $(BIN_DIR)/* $(OUT_DIR)/*.o *.core

.PHONY: clean

# $(OUT_DIR)/%.o: %.c $(DEPS)
# 	$(CC) -c -o $@ $< $(CFLAGS)
$(OUT_DIR)/kinetic_allocator.o: $(LIB_DIR)/kinetic_allocator.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/kinetic_types_internal.o: $(LIB_DIR)/kinetic_types_internal.c $(LIB_DEPS)
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
$(OUT_DIR)/byte_array.o: $(LIB_DIR)/byte_array.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)
$(OUT_DIR)/socket99.o: $(SOCKET99)/socket99.c $(SOCKET99)/socket99.h
	$(CC) -c -o $@ $< $(CFLAGS) -I$(SOCKET99)
$(OUT_DIR)/protobuf-c.o: $(PROTOBUFC)/protobuf-c/protobuf-c.c $(PROTOBUFC)/protobuf-c/protobuf-c.h
	$(CC) -c -o $@ $< -std=c99 -fPIC -g -Wall $(OPTIMIZE) -Wno-unused-parameter -I$(PROTOBUFC)
$(OUT_DIR)/kinetic_client.o: $(LIB_DIR)/kinetic_client.c $(LIB_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIB_INCS)



#-------------------------------------------------------------------------------
# Static and Dynamic Library Build Support
#-------------------------------------------------------------------------------

PREFIX ?= /usr/local
KINETIC_SO_DEV = $(BIN_DIR)/lib$(KINETIC_LIB_NAME).so
KINETIC_SO_RELEASE = $(PREFIX)/lib$(KINETIC_LIB_NAME).so

$(KINETIC_LIB): $(LIB_OBJS) $(KINETIC_LIB_OTHER_DEPS)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Building static library: $(KINETIC_LIB)
	@echo --------------------------------------------------------------------------------
	ar -rcs $@ $(LIB_OBJS)
	ar -t $@

$(KINETIC_SO_DEV): $(LIB_OBJS) $(KINETIC_LIB_OTHER_DEPS)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Building dynamic library: $(KINETIC_SO_DEV)
	@echo --------------------------------------------------------------------------------
	$(CC) -o $@ -shared $(LDFLAGS) $(LIB_OBJS)



#-------------------------------------------------------------------------------
# Installation Support
#-------------------------------------------------------------------------------

API_NAME = kinetic_client
INSTALL ?= install
RM ?= rm

install: $(KINETIC_LIB) $(KINETIC_SO_DEV)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Installing $(PROJECT) v$(VERSION) into $(PREFIX)
	@echo --------------------------------------------------------------------------------
	@echo
	$(INSTALL) -d $(PREFIX)/lib/
	$(INSTALL) -c $(KINETIC_LIB) $(PREFIX)/lib/
	$(INSTALL) -d $(PREFIX)/include/
	$(INSTALL) -c $(PUB_INC)/$(API_NAME).h $(PREFIX)/include/
	$(INSTALL) -c $(PUB_INC)/kinetic_types.h $(PREFIX)/include/

uninstall:
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Uninstalling $(PROJECT) from $(PREFIX)
	@echo --------------------------------------------------------------------------------
	@echo
	$(RM) -f $(PREFIX)/lib/lib$(PROJECT)*.a
	$(RM) -f $(PREFIX)/lib/lib$(PROJECT)*.so
	$(RM) -f $(PREFIX)/include/${PUBLIC_API}.h
	$(RM) -f $(PREFIX)/include/kinetic_types.h
	$(RM) -f $(PREFIX)/include/kinetic_proto.h
	$(RM) -f $(PREFIX)/include/protobuf-c/protobuf-c.h
	$(RM) -f $(PREFIX)/include/protobuf-c.h


#===============================================================================
# Test Utility Build Support
#===============================================================================

UTILITY = kinetic-c-util
UTIL_DIR = ./src/utility
UTIL_EXEC = $(BIN_DIR)/$(UTILITY)
UTIL_OBJ = $(OUT_DIR)/main.o
UTIL_LDFLAGS += -lm -l ssl $(KINETIC_LIB) -l crypto

$(UTIL_OBJ): $(UTIL_DIR)/main.c
	$(CC) -c -o $@ $< $(CFLAGS) -I$(PUB_INC) -I$(UTIL_DIR)

$(UTIL_EXEC): $(UTIL_OBJ) $(KINETIC_LIB)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Building development test utility: $(UTIL_EXEC)
	@echo --------------------------------------------------------------------------------
	$(CC) -o $@ $< $(CFLAGS) $(UTIL_LDFLAGS) $(KINETIC_LIB)

utility: $(UTIL_EXEC)

build: $(KINETIC_LIB) $(KINETIC_SO_DEV) utility

#-------------------------------------------------------------------------------
# Support for Simulator and Exection of Test Utility
#-------------------------------------------------------------------------------
# JAVA=$(JAVA_HOME)/bin/java
SIM_JARS_PREFIX = vendor/kinetic-java/kinetic-simulator-0.7.0.2-kinetic-proto-2.0.6-SNAPSHOT
CLASSPATH = $(JAVA_HOME)/lib/tools.jar:$(SIM_JARS_PREFIX)-jar-with-dependencies.jar:$(SIM_JARS_PREFIX)-sources.jar:$(SIM_JARS_PREFIX).jar
SIM_RUNNER = com.seagate.kinetic.simulator.internal.SimulatorRunner
SIM_ADMIN = com.seagate.kinetic.admin.cli.KineticAdminCLI

run: $(UTIL_EXEC)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Running test utility: $(UTIL_EXEC)
	@echo --------------------------------------------------------------------------------
	@sleep 2
	exec java -classpath "$(CLASSPATH)" $(SIM_RUNNER) "$@" &
	@sleep 5
	exec java -classpath "$(CLASSPATH)" $(SIM_ADMIN) -setup -erase true
	$(UTIL_EXEC) noop put get delete
	exec pkill -f 'java.*kinetic-simulator'
	@echo
	@echo Test Utility integration tests w/ kinetic-c lib passed!
	@echo
