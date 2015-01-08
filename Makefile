SHELL := /bin/bash

#===============================================================================
# Configuration of Shared Paths
#===============================================================================
OUT_DIR = ./obj
BIN_DIR = ./bin
PUB_INC = ./include

#-------------------------------------------------------------------------------
# SSL/TLS Library Options
#-------------------------------------------------------------------------------

# FIXME: Currently OSX/homebrew specific, rework before integration.
OPENSSL_PATH ?=	.

#===============================================================================
# Shared Build Variables
#===============================================================================
CC ?= gcc
OPTIMIZE = -O3
SYSTEM_TEST_HOST ?= localhost
SESSION_HMAC_KEY ?= \"asdfasdf\"
SESSION_PIN ?= \"1234\"
WARN = -Wall -Wextra -Wstrict-prototypes -Wcast-align -pedantic -Wno-missing-field-initializers -Werror=strict-prototypes
CDEFS += -D_POSIX_C_SOURCE=199309L -D_C99_SOURCE=1 -DSYSTEM_TEST_HOST=\"${SYSTEM_TEST_HOST}\" -DCLUSTER_VERSION=${CLUSTER_VERSION}
CFLAGS += -std=c99 -fPIC -g $(WARN) $(CDEFS) $(OPTIMIZE)
LDFLAGS += -lm -L${OPENSSL_PATH}/lib -lcrypto -lssl -lpthread

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
THREADPOOL_PATH = ${LIB_DIR}/threadpool
BUS_PATH = ${LIB_DIR}/bus

KINETIC_LIB_NAME = $(PROJECT).$(VERSION)
KINETIC_LIB = $(BIN_DIR)/lib$(KINETIC_LIB_NAME).a
LIB_INCS = -I$(LIB_DIR) -I$(PUB_INC) -I$(PROTOBUFC) -I$(SOCKET99) -I$(VENDOR) \
	-I$(THREADPOOL_PATH) -I$(BUS_PATH) -I${OPENSSL_PATH}/include

C_SRC=${LIB_DIR}/*.[ch] $(SOCKET99)/socket99.[ch] $(PROTOBUFC)/protobuf-c/protobuf-c.[ch]

LIB_OBJS = \
	$(OUT_DIR)/socket99.o \
	$(OUT_DIR)/protobuf-c.o \
	$(OUT_DIR)/kinetic_allocator.o \
	$(OUT_DIR)/kinetic_nbo.o \
	$(OUT_DIR)/kinetic_operation.o \
	$(OUT_DIR)/kinetic_pdu.o \
	$(OUT_DIR)/kinetic_auth.o \
	$(OUT_DIR)/kinetic_pdu_unpack.o \
	$(OUT_DIR)/kinetic_proto.o \
	$(OUT_DIR)/kinetic_socket.o \
	$(OUT_DIR)/kinetic_message.o \
	$(OUT_DIR)/kinetic_logger.o \
	$(OUT_DIR)/kinetic_hmac.o \
	$(OUT_DIR)/kinetic_controller.o \
	$(OUT_DIR)/kinetic_device_info.o \
	$(OUT_DIR)/kinetic_serial_allocator.o \
	$(OUT_DIR)/kinetic_session.o \
	$(OUT_DIR)/kinetic_types_internal.o \
	$(OUT_DIR)/kinetic_types.o \
	$(OUT_DIR)/byte_array.o \
	$(OUT_DIR)/kinetic_client.o \
	$(OUT_DIR)/kinetic_admin_client.o \
	$(OUT_DIR)/threadpool.o \
	$(OUT_DIR)/bus.o \
	$(OUT_DIR)/bus_ssl.o \
	$(OUT_DIR)/casq.o \
	$(OUT_DIR)/listener.o \
	$(OUT_DIR)/sender.o \
	$(OUT_DIR)/util.o \
	$(OUT_DIR)/yacht.o \



KINETIC_LIB_OTHER_DEPS = Makefile Rakefile $(VERSION_FILE)


default: makedirs $(KINETIC_LIB)

makedirs:
	@echo; mkdir -p ./bin/examples &> /dev/null; mkdir -p ./bin/unit &> /dev/null; mkdir -p ./bin/systest &> /dev/null; mkdir -p ./out &> /dev/null

all: default test system_tests test_internals run examples

clean: makedirs update_git_submodules
	rm -rf ./bin/**/*
	rm ./bin/*.*
	rm -f $(OUT_DIR)/*.o $(OUT_DIR)/*.a *.core *.log
	bundle exec rake clobber
	-./vendor/kinetic-simulator/stopSimulator.sh &> /dev/null;
	cd ${SOCKET99} && make clean
	cd ${LIB_DIR}/threadpool && make clean
	cd ${LIB_DIR}/bus && make clean

update_git_submodules:
	git submodule update --init

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

$(OUT_DIR)/threadpool.o: ${LIB_DIR}/threadpool/threadpool.c ${LIB_DIR}/threadpool/threadpool.h
	$(CC) -o $@ -c $< $(CFLAGS)

$(OUT_DIR)/%.o: ${LIB_DIR}/bus/%.c ${LIB_DIR}/bus/%.h
	$(CC) -o $@ -c $< $(CFLAGS) -I${THREADPOOL_PATH} -I${BUS_PATH} ${LIB_INCS}

${OUT_DIR}/*.o: src/lib/kinetic_types_internal.h


ci: uninstall all test_internals install
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

test_internals: test_threadpool test_bus

test_threadpool:
	cd ${LIB_DIR}/threadpool && make test

test_bus: test_threadpool ${OUT_DIR}/libsocket99.a ${OUT_DIR}/libthreadpool.a
	cd ${LIB_DIR}/bus && make test

#-------------------------------------------------------------------------------
# Internal Libraries
#-------------------------------------------------------------------------------

${OUT_DIR}/libsocket99.a: ${SOCKET99}/*.[ch]
	cd ${SOCKET99} && make all
	cp ${SOCKET99}/libsocket99.a $@

${OUT_DIR}/libthreadpool.a: ${LIB_DIR}/threadpool/*.[ch]
	cd ${LIB_DIR}/threadpool && make all
	cp ${LIB_DIR}/threadpool/libthreadpool.a $@


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
	$(INSTALL) -c $(PUB_INC)/kinetic_client.h $(PREFIX)/include/
	$(INSTALL) -c $(PUB_INC)/kinetic_admin_client.h $(PREFIX)/include/
	$(INSTALL) -c $(PUB_INC)/kinetic_types.h $(PREFIX)/include/
	$(INSTALL) -c $(PUB_INC)/byte_array.h $(PREFIX)/include/

uninstall:
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Uninstalling $(PROJECT) from $(PREFIX)
	@echo --------------------------------------------------------------------------------
	@echo
	$(RM) -f $(PREFIX)${LIBDIR}/lib$(PROJECT)*.a
	$(RM) -f $(PREFIX)${LIBDIR}/lib$(PROJECT)*.so
	$(RM) -f $(PREFIX)/include/kinetic_client.h
	$(RM) -f $(PREFIX)/include/kinetic_admin_client.h
	$(RM) -f $(PREFIX)/include/kinetic_types.h
	$(RM) -f $(PREFIX)/include/byte_array.h

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

.PHONY: update_simulator erase_simulator stop_simulator


#===============================================================================
# Unity Test Framework Support
#===============================================================================
UNITY_INC = ./vendor/unity/src
UNITY_SRC = ./vendor/unity/src/unity.c

test: Rakefile $(LIB_OBJS)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Testing $(PROJECT)
	@echo --------------------------------------------------------------------------------
	bundle install
	bundle exec rake test_all


#===============================================================================
# CMock Tests
#===============================================================================

UNIT_SRC = ./test/unit
UNIT_OUT = $(BIN_DIR)/unit
UNIT_LDFLAGS += -lm -l ssl $(KINETIC_LIB) -l crypto -l pthread
UNIT_WARN = -Wall -Wextra -Wstrict-prototypes -pedantic -Wno-missing-field-initializers -Werror=strict-prototypes
UNIT_CFLAGS += -std=c99 -fPIC -g $(UNIT_WARN) $(CDEFS) $(OPTIMIZE) -DTEST

unit_sources = $(wildcard $(UNIT_SRC)/*.c)
unit_executables = $(patsubst $(UNIT_SRC)/%.c,$(UNIT_OUT)/run_%,$(unit_sources))
unit_results = $(patsubst $(UNIT_OUT)/run_%,$(UNIT_OUT)/%.log,$(unit_executables))
unit_passfiles = $(patsubst $(UNIT_OUT)/run_%,$(UNIT_OUT)/%.testpass,$(unit_executables))
unit_names = $(patsubst $(UNIT_OUT)/run_%,%,$(unit_executables))

list_unit_tests:
	echo $(unit_names)

$(UNIT_OUT)/%_runner.c: $(UNIT_SRC)/%.c
	./test/support/generate_unit_runner.sh $< > $@

$(UNIT_OUT)/run_%: $(UNIT_SRC)/%.c $(UNIT_OUT)/%_runner.c $(KINETIC_LIB)
	@echo
	@echo ================================================================================
	@echo System test: '$<'
	@echo --------------------------------------------------------------------------------
	$(CC) -o $@ $< $(word 2,$^) ./test/support/unit_unit_fixture.c $(UNITY_SRC) $(UNIT_CFLAGS) $(LIB_INCS) -I$(UNITY_INC) -I./test/support $(UNIT_LDFLAGS) $(KINETIC_LIB)

$(UNIT_OUT)/%.testpass : $(UNIT_OUT)/run_%
	./scripts/runSystemTest.sh $*

$(unit_names) : % : $(UNIT_OUT)/%.testpass

unit_tests: start_simulator $(unit_passfiles)


#===============================================================================
# System Tests
#===============================================================================


SYSTEST_SRC = ./test/system
SYSTEST_OUT = $(BIN_DIR)/systest
SYSTEST_LDFLAGS += -lm -L${OPENSSL_PATH}/lib -lssl -lcrypto $(KINETIC_LIB) -l pthread
SYSTEST_WARN = -Wall -Wextra -Wstrict-prototypes -pedantic -Wno-missing-field-initializers -Werror=strict-prototypes -Wno-nonnull
SYSTEST_CFLAGS += -std=c99 -fPIC -g $(SYSTEST_WARN) $(CDEFS) $(OPTIMIZE) -DTEST

systest_sources = $(wildcard $(SYSTEST_SRC)/*.c)
systest_executables = $(patsubst $(SYSTEST_SRC)/%.c,$(SYSTEST_OUT)/run_%,$(systest_sources))
systest_results = $(patsubst $(SYSTEST_OUT)/run_%,$(SYSTEST_OUT)/%.log,$(systest_executables))
systest_passfiles = $(patsubst $(SYSTEST_OUT)/run_%,$(SYSTEST_OUT)/%.testpass,$(systest_executables))
systest_names = $(patsubst $(SYSTEST_OUT)/run_%,%,$(systest_executables))

.SECONDARY: $(systest_executables)

list_system_tests:
	echo $(systest_names)

$(SYSTEST_OUT)/%_runner.c: $(SYSTEST_SRC)/%.c
	./test/support/generate_test_runner.sh $< > $@

$(SYSTEST_OUT)/run_%: $(SYSTEST_SRC)/%.c $(SYSTEST_OUT)/%_runner.c $(KINETIC_LIB)
	@echo
	@echo ================================================================================
	@echo System test: '$<'
	@echo --------------------------------------------------------------------------------
	$(CC) -o $@ $< $(word 2,$^) ./test/support/system_test_fixture.c $(UNITY_SRC) $(SYSTEST_CFLAGS) $(LIB_INCS) -I$(UNITY_INC) -I./test/support $(SYSTEST_LDFLAGS) $(KINETIC_LIB)

$(SYSTEST_OUT)/%.testpass : $(SYSTEST_OUT)/run_%
	./scripts/runSystemTest.sh $*

$(systest_names) : % : $(SYSTEST_OUT)/%.testpass
system_tests: start_simulator $(systest_passfiles)

.SECONDARY: $(systest_executables) $(systest_sources)


# valgrind_systest_%: $(SYSTEST_OUT)/%
# 	@echo
# 	@echo ================================================================================
# 	@echo Executing system test: '$<'
# 	@echo --------------------------------------------------------------------------------;
# 	${VALGRIND} ${VALGRIND_ARGS} $<
# 	@echo ================================================================================
# 	@echo


#===============================================================================
# Test Utility Build Support
#===============================================================================

UTILITY = kinetic-c-util
UTIL_DIR = ./src/utility
UTIL_EXEC = $(BIN_DIR)/$(UTILITY)
UTIL_OBJ = $(OUT_DIR)/main.o
UTIL_LDFLAGS += -lm -L${OPENSSL_PATH}/lib -lssl $(KINETIC_LIB) -lcrypto -lpthread

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
JAVA_HOME ?= /usr
JAVA_BIN = $(JAVA_HOME)/bin/java
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

example_sources = $(wildcard $(EXAMPLE_SRC)/*.c)
example_executables = $(patsubst $(EXAMPLE_SRC)/%.c,$(BIN_DIR)/examples/%,$(example_sources))

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
	run_example_get_key_range \
	stop_simulator

valgrind_examples: setup_examples \
	start_simulator \
	valgrind_example_write_file_blocking \
	valgrind_example_write_file_blocking_threads \
	valgrind_example_write_file_nonblocking \
	valgrind_example_write_file_nonblocking_threads \
	valgrind_example_get_key_range \
	stop_simulator
