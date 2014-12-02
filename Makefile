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
WARN = -Wall -Wextra -Wstrict-prototypes -Wcast-align -pedantic -Wno-missing-field-initializers
CDEFS += -D_POSIX_C_SOURCE=199309L -D_C99_SOURCE=1
CFLAGS += -std=c99 -fPIC -g $(WARN) $(CDEFS) $(OPTIMIZE)
LDFLAGS += -lm -lcrypto -lssl -lpthread

#===============================================================================
# Kinetic-C Library Build Support
#===============================================================================

PROJECT = kinetic-c-client

PREFIX ?= /usr/local
LIBDIR ?= /lib

LIB_DIR = ./src/lib
VENDOR = ./vendor
PROTOBUFC = $(VENDOR)/protobuf-c
SOCKET99 = $(VENDOR)/socket99
VERSION_FILE = ./config/VERSION
VERSION = ${shell head -n1 $(VERSION_FILE)}

KINETIC_LIB_NAME = $(PROJECT).$(VERSION)
KINETIC_LIB = $(BIN_DIR)/lib$(KINETIC_LIB_NAME).a
LIB_INCS = -I$(LIB_DIR) -I$(PUB_INC) -I$(PROTOBUFC) -I$(SOCKET99) -I$(VENDOR)

C_SRC=${LIB_DIR}/*.[ch] $(SOCKET99)/socket99.[ch] $(PROTOBUFC)/protobuf-c/protobuf-c.[ch]

LIB_OBJS = \
	$(OUT_DIR)/socket99.o \
	$(OUT_DIR)/protobuf-c.o \
	$(OUT_DIR)/kinetic_allocator.o \
	$(OUT_DIR)/kinetic_nbo.o \
	$(OUT_DIR)/kinetic_operation.o \
	$(OUT_DIR)/kinetic_pdu.o \
	$(OUT_DIR)/kinetic_proto.o \
	$(OUT_DIR)/kinetic_socket.o \
	$(OUT_DIR)/kinetic_message.o \
	$(OUT_DIR)/kinetic_logger.o \
	$(OUT_DIR)/kinetic_hmac.o \
	$(OUT_DIR)/kinetic_controller.o \
	$(OUT_DIR)/kinetic_device_info.o \
	$(OUT_DIR)/kinetic_serial_allocator.o \
	$(OUT_DIR)/kinetic_connection.o \
	$(OUT_DIR)/kinetic_types_internal.o \
	$(OUT_DIR)/kinetic_types.o \
	$(OUT_DIR)/byte_array.o \
	$(OUT_DIR)/kinetic_client.o

KINETIC_LIB_OTHER_DEPS = Makefile Rakefile $(VERSION_FILE)

default: makedirs $(KINETIC_LIB)

makedirs:
	@echo; mkdir -p ./bin/examples &> /dev/null; mkdir -p ./out &> /dev/null

all: default test run examples

clean: makedirs
	rm -rf ./bin/**/*
	rm -f $(OUT_DIR)/*.o *.core *.log
	bundle exec rake clobber
	git submodule update --init
	-./vendor/kinetic-simulator/stopSimulator.sh &> /dev/null;

TAGS: ${C_SRC} Makefile
	@find . -name "*.[ch]" | grep -v vendor | grep -v build | xargs etags

.PHONY: clean run ci

${C_SRC}: Makefile

.POSIX:

# Source files should depend on corresponding .h, if present.
COMPILE_SOURCE=$(CC) -c -o $@ ${LIB_DIR}/$*.c $(CFLAGS) $(LIB_INCS)
${OUT_DIR}/%.o: ${LIB_DIR}/%.c ${LIB_DIR}/%.h ${PUB_INC}/*.h Makefile
	${COMPILE_SOURCE}
${OUT_DIR}/%.o: ${LIB_DIR}/%.c Makefile ${PUB_INC}/%.h Makefile
	${COMPILE_SOURCE}

# Sources with atypical paths / dependencies
$(OUT_DIR)/socket99.o: $(SOCKET99)/socket99.c $(SOCKET99)/socket99.h
	$(CC) -c -o $@ $< $(CFLAGS) -I$(SOCKET99)
$(OUT_DIR)/protobuf-c.o: $(PROTOBUFC)/protobuf-c/protobuf-c.c $(PROTOBUFC)/protobuf-c/protobuf-c.h
	$(CC) -c -o $@ $< -std=c99 -fPIC -g -Wall -Wno-unused-parameter $(OPTIMIZE) -I$(PROTOBUFC)
${OUT_DIR}/kinetic_types.o: ${LIB_DIR}/kinetic_types_internal.h


ci: uninstall all install
	@echo
	@echo --------------------------------------------------------------------------------
	@echo $(PROJECT) build completed successfully!
	@echo --------------------------------------------------------------------------------
	@echo $(PROJECT) v$(VERSION) is in working order
	@echo


#-------------------------------------------------------------------------------
# Test Support
#-------------------------------------------------------------------------------

test: Rakefile $(LIB_OBJS)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Testing $(PROJECT)
	@echo --------------------------------------------------------------------------------
	bundle install
	bundle exec rake test_all

JAVA_HOME ?= /usr
JAVA_BIN = $(JAVA_HOME)/bin/java

.PHONY: test


#-------------------------------------------------------------------------------
# Static and Dynamic Library Build Support
#-------------------------------------------------------------------------------

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
	$(INSTALL) -d $(PREFIX)${LIBDIR}
	$(INSTALL) -c $(KINETIC_LIB) $(PREFIX)${LIBDIR}/
	$(INSTALL) -d $(PREFIX)/include/
	$(INSTALL) -c $(PUB_INC)/$(API_NAME).h $(PREFIX)/include/
	$(INSTALL) -c $(PUB_INC)/kinetic_types.h $(PREFIX)/include/

uninstall:
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Uninstalling $(PROJECT) from $(PREFIX)
	@echo --------------------------------------------------------------------------------
	@echo
	$(RM) -f $(PREFIX)${LIBDIR}/lib$(PROJECT)*.a
	$(RM) -f $(PREFIX)${LIBDIR}/lib$(PROJECT)*.so
	$(RM) -f $(PREFIX)/include/${API_NAME}.h
	$(RM) -f $(PREFIX)/include/kinetic_types.h
	$(RM) -f $(PREFIX)/include/kinetic_proto.h
	$(RM) -f $(PREFIX)/include/protobuf-c/protobuf-c.h
	$(RM) -f $(PREFIX)/include/protobuf-c.h

.PHONY: install uninstall


#===============================================================================
# Java Simulator Support
#===============================================================================

update_simulator:
	cd vendor/kinetic-java; mvn clean package; cd -
	cp vendor/kinetic-java/kinetic-simulator/target/*.jar vendor/kinetic-java-simulator/

start_simulator:
	./vendor/kinetic-simulator/startSimulator.sh

stop_simulator:
	./vendor/kinetic-simulator/stopSimulator.sh

.PHONY: update_simulator start_simulator erase_simulator stop_simulator


#===============================================================================
# Test Utility Build Support
#===============================================================================

UTILITY = kinetic-c-util
UTIL_DIR = ./src/utility
UTIL_EXEC = $(BIN_DIR)/$(UTILITY)
UTIL_OBJ = $(OUT_DIR)/main.o
UTIL_LDFLAGS += -lm -lssl $(KINETIC_LIB) -lcrypto -lpthread

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

run: $(UTIL_EXEC) start_simulator
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Running test utility: $(UTIL_EXEC)
	@echo --------------------------------------------------------------------------------
	@echo
	$(UTIL_EXEC) instanterase
	$(UTIL_EXEC) noop
	exec $(UTIL_EXEC) put get delete
	@echo
	@echo Test Utility integration tests w/ kinetic-c lib passed!
	@echo

#===============================================================================
# Standalone Example Executables
#===============================================================================

EXAMPLE_SRC = ./src/examples
EXAMPLE_LDFLAGS += -lm -l ssl $(KINETIC_LIB) -l crypto -l pthread
EXAMPLES = write_file_blocking
VALGRIND = valgrind
VALGRIND_ARGS = --track-origins=yes #--leak-check=full

example_sources = $(wildcard $(EXAMPLE_SRC)/*.c)
example_executables = $(patsubst $(EXAMPLE_SRC)/%.c,$(BIN_DIR)/examples/%,$(example_sources))

list_examples:
	echo $(example_executables)

$(BIN_DIR)/examples/%: $(EXAMPLE_SRC)/%.c $(KINETIC_LIB)
	@echo
	@echo ================================================================================
	@echo Building example: '$<'
	@echo --------------------------------------------------------------------------------
	$(CC) -o $@ $< $(CFLAGS) -I$(PUB_INC) $(UTIL_LDFLAGS) $(KINETIC_LIB)
	@echo ================================================================================
	@echo

build_examples: $(example_executables)

test_example_%: $(BIN_DIR)/examples/%
	@echo
	@echo ================================================================================
	@echo Executing example: '$<'
	@echo --------------------------------------------------------------------------------;
	$<
	@echo ================================================================================
	@echo
	./vendor/kinetic-simulator/stopSimulator.sh
test_example_%: start_simulator

run_example_%: $(BIN_DIR)/examples/%
	@echo
	@echo ================================================================================
	@echo Executing example: '$<'
	@echo --------------------------------------------------------------------------------;
	$<
	@echo ================================================================================
	@echo

valgrind_example_%: $(BIN_DIR)/examples/%
	@echo
	@echo ================================================================================
	@echo Executing example: '$<'
	@echo --------------------------------------------------------------------------------;
	${VALGRIND} ${VALGRIND_ARGS} $<
	@echo ================================================================================
	@echo

setup_examples: $(example_executables) \
	build_examples

examples: setup_examples \
	start_simulator \
	run_example_write_file_blocking \
	run_example_write_file_blocking_threads \
	run_example_write_file_nonblocking \
	run_example_write_file_nonblocking_threads \
	stop_simulator

valgrind_examples: setup_examples \
	start_simulator \
	valgrind_example_write_file_blocking \
	valgrind_example_write_file_blocking_threads \
	valgrind_example_write_file_nonblocking \
	valgrind_example_write_file_nonblocking_threads \
	stop_simulator
