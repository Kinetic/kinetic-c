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

# This may need to be set if OpenSSL is in a nonstandard place.
OPENSSL_PATH ?=	.

#===============================================================================
# Shared Build Variables
#===============================================================================
CC ?= gcc
OPTIMIZE = -O3
KINETIC_HOST1 ?= localhost
SESSION_HMAC_KEY ?= asdfasdf
SESSION_PIN ?= 1234
WARN = -Wall -Wextra -Wstrict-prototypes -Wcast-align -pedantic
WARN += -Wno-missing-field-initializers -Werror=strict-prototypes -Wshadow
WARN += -Werror
CDEFS += -D_POSIX_C_SOURCE=199309L -D_C99_SOURCE=1
CFLAGS += -std=c99 -fPIC -g $(WARN) $(CDEFS) $(OPTIMIZE)
LDFLAGS += -lm -L${OPENSSL_PATH}/lib -lcrypto -lssl -lpthread -ljson-c
NUM_SIMS ?= 2

#===============================================================================
# Kinetic-C Library Build Support
#===============================================================================

PROJECT = kinetic-c-client
PREFIX ?= /usr/local
LIBDIR ?= /lib
LIB_DIR = ./src/lib
VERSION_INFO = $(LIB_DIR)/kinetic_version_info.h
VENDOR = ./vendor
PROTOBUFC = $(VENDOR)/protobuf-c
SOCKET99 = $(VENDOR)/socket99
JSONC = $(VENDOR)/json-c
VERSION_FILE = ./config/VERSION
VERSION = ${shell head -n1 $(VERSION_FILE)}
THREADPOOL_PATH = ${LIB_DIR}/threadpool
BUS_PATH = ${LIB_DIR}/bus
JSONC_LIB = ${OUT_DIR}/libjson-c.a

KINETIC_LIB_NAME = $(PROJECT).$(VERSION)
KINETIC_LIB = $(BIN_DIR)/lib$(KINETIC_LIB_NAME).a
LIB_INCS = -I$(LIB_DIR) -I$(PUB_INC) -I$(PROTOBUFC) -I$(SOCKET99) -I$(VENDOR) \
	-I$(JSONC) -I$(THREADPOOL_PATH) -I$(BUS_PATH) -I${OPENSSL_PATH}/include

C_SRC=${LIB_DIR}/*.[ch] $(SOCKET99)/socket99.[ch] $(PROTOBUFC)/protobuf-c/protobuf-c.[ch]

LIB_OBJS = \
	$(OUT_DIR)/socket99.o \
	$(OUT_DIR)/protobuf-c.o \
	$(OUT_DIR)/kinetic_allocator.o \
	$(OUT_DIR)/kinetic_nbo.o \
	$(OUT_DIR)/kinetic_operation.o \
	$(OUT_DIR)/kinetic_callbacks.o \
	$(OUT_DIR)/kinetic_builder.o \
	$(OUT_DIR)/kinetic_request.o \
	$(OUT_DIR)/kinetic_response.o \
	$(OUT_DIR)/kinetic_bus.o \
	$(OUT_DIR)/kinetic_auth.o \
	$(OUT_DIR)/kinetic_pdu_unpack.o \
	$(OUT_DIR)/kinetic.pb-c.o \
	$(OUT_DIR)/kinetic_socket.o \
	$(OUT_DIR)/kinetic_message.o \
	$(OUT_DIR)/kinetic_logger.o \
	$(OUT_DIR)/kinetic_hmac.o \
	$(OUT_DIR)/kinetic_controller.o \
	$(OUT_DIR)/kinetic_device_info.o \
	$(OUT_DIR)/kinetic_session.o \
	$(OUT_DIR)/kinetic_types_internal.o \
	$(OUT_DIR)/kinetic_types.o \
	$(OUT_DIR)/kinetic_memory.o \
	$(OUT_DIR)/kinetic_semaphore.o \
	$(OUT_DIR)/kinetic_countingsemaphore.o \
	$(OUT_DIR)/kinetic_resourcewaiter.o \
	$(OUT_DIR)/kinetic_acl.o \
	$(OUT_DIR)/byte_array.o \
	$(OUT_DIR)/kinetic_client.o \
	$(OUT_DIR)/kinetic_admin_client.o \
	$(OUT_DIR)/threadpool.o \
	$(OUT_DIR)/bus.o \
	$(OUT_DIR)/bus_poll.o \
	$(OUT_DIR)/bus_ssl.o \
	$(OUT_DIR)/listener.o \
	$(OUT_DIR)/listener_cmd.o \
	$(OUT_DIR)/listener_helper.o \
	$(OUT_DIR)/listener_io.o \
	$(OUT_DIR)/listener_task.o \
	$(OUT_DIR)/send.o \
	$(OUT_DIR)/send_helper.o \
	$(OUT_DIR)/syscall.o \
	$(OUT_DIR)/util.o \
	$(OUT_DIR)/yacht.o \

KINETIC_LIB_OTHER_DEPS = Makefile Rakefile $(VERSION_FILE) $(VERSION_INFO)


default: makedirs $(KINETIC_LIB)

makedirs:
	@echo; mkdir -p ./bin/examples &> /dev/null; mkdir -p ./bin/unit &> /dev/null; mkdir -p ./bin/systest &> /dev/null; mkdir -p ./out &> /dev/null

all: default test system_tests test_internals run examples

clean: makedirs
	rm -rf ./bin/*.a ./bin/*.so ./bin/kinetic-c-util $(DISCOVERY_UTIL_EXEC)
	rm -rf ./bin/**/*
	rm -f ./bin/*.*
	rm -f $(OUT_DIR)/*.o $(OUT_DIR)/*.a *.core *.log
	if rake --version > /dev/null 2>&1; then if bundle --version > /dev/null 2>&1; then bundle exec rake clobber; fi; fi
	cd ${SOCKET99} && make clean
	cd ${LIB_DIR}/threadpool && make clean
	cd ${LIB_DIR}/bus && make clean
	if [ -f ${JSONC}/Makefile ]; then cd ${JSONC} && make clean; fi;

# Setup version info generation and corresponding dependencies
$(VERSION_INFO): $(VERSION_FILE)
	sh scripts/generate_version_info.sh
$(OUT_DIR)/kinetic_logger.o: $(VERSION_INFO)
$(OUT_DIR)/kinetic_client.o: $(VERSION_INFO)

config: makedirs update_git_submodules

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
	$(CC) -c -o $@ $< $(CFLAGS) -Wno-shadow -I$(SOCKET99)
$(OUT_DIR)/protobuf-c.o: $(PROTOBUFC)/protobuf-c/protobuf-c.c $(PROTOBUFC)/protobuf-c/protobuf-c.h
	$(CC) -c -o $@ $< -std=c99 -fPIC -g -Wall -Werror -Wno-unused-parameter $(OPTIMIZE) -I$(PROTOBUFC)
${OUT_DIR}/kinetic_types.o: ${LIB_DIR}/kinetic_types_internal.h
${OUT_DIR}/bus.o: ${LIB_DIR}/bus/bus_types.h
${OUT_DIR}/sender.o: ${LIB_DIR}/bus/sender_internal.h
${OUT_DIR}/sender_helper.o: ${LIB_DIR}/bus/sender_internal.h
${OUT_DIR}/listener.o: ${LIB_DIR}/bus/listener_internal.h
${OUT_DIR}/listener_cmd.o: ${LIB_DIR}/bus/listener_internal.h
${OUT_DIR}/listener_helper.o: ${LIB_DIR}/bus/listener_internal.h
${OUT_DIR}/listener_io.o: ${LIB_DIR}/bus/listener_internal.h
${OUT_DIR}/listener_task.o: ${LIB_DIR}/bus/listener_internal.h

$(OUT_DIR)/threadpool.o: ${LIB_DIR}/threadpool/threadpool.c ${LIB_DIR}/threadpool/threadpool.h
	$(CC) -o $@ -c $< $(CFLAGS)

$(OUT_DIR)/%.o: ${LIB_DIR}/bus/%.c ${LIB_DIR}/bus/%.h
	$(CC) -o $@ -c $< $(CFLAGS) -I${THREADPOOL_PATH} -I${BUS_PATH} ${LIB_INCS}

${OUT_DIR}/*.o: src/lib/kinetic_types_internal.h

ci: stop_sims start_sims all stop_sims
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Testing install/uninstall of kinetic-c
	@echo --------------------------------------------------------------------------------
	sudo make install
	sudo make uninstall
	@echo
	@echo --------------------------------------------------------------------------------
	@echo $(PROJECT) build completed successfully!
	@echo --------------------------------------------------------------------------------
	@echo $(PROJECT) v$(VERSION) is in working order!
	@echo

apply_license:
	scripts/apply_license.sh

#-------------------------------------------------------------------------------
# json-c
#-------------------------------------------------------------------------------

json: ${JSONC_LIB}

$(OUT_DIR)/kinetic_acl.o: ${JSONC_LIB}

json_install: ${JSONC_LIB}
	cd ${JSONC} && \
	make install

json_uninstall:
	if [ -f ${JSONC}/Makefile ]; then cd ${JSONC} && make uninstall; fi;

.PHONY: json_install json_uninstall

${JSONC}/Makefile:
	cd ${JSONC} && \
	sh autogen.sh && \
	./configure

${JSONC}/.libs/libjson-c.a: ${JSONC}/Makefile
	cd ${JSONC} && \
	make libjson-c.la

${JSONC_LIB}: ${JSONC}/.libs/libjson-c.a
	cp ${JSONC}/.libs/libjson-c.a ${JSONC_LIB}


#-------------------------------------------------------------------------------
# Test Support
#-------------------------------------------------------------------------------

test: Rakefile $(LIB_OBJS)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Testing $(PROJECT)
	@echo --------------------------------------------------------------------------------
	bundle install
	bundle exec rake test:delta

JAVA_HOME ?= /usr
JAVA_BIN = $(JAVA_HOME)/bin/java

.PHONY: test

test_internals: test_threadpool

test_threadpool:
	cd ${LIB_DIR}/threadpool && make test

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

$(KINETIC_LIB): $(KINETIC_LIB_OTHER_DEPS) $(LIB_OBJS) $(JSONC_LIB)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Building static library: $(KINETIC_LIB)
	@echo --------------------------------------------------------------------------------
	ar -rcs $@ $(LIB_OBJS)
	ar -t $@

$(KINETIC_SO_DEV): $(LIB_OBJS) $(KINETIC_LIB_OTHER_DEPS) $(JSONC_LIB)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Building dynamic library: $(KINETIC_SO_DEV)
	@echo --------------------------------------------------------------------------------
	$(CC) -o $@ -shared $(LDFLAGS) $(LIB_OBJS)


#-------------------------------------------------------------------------------
# Installation Support
#-------------------------------------------------------------------------------

INSTALL ?= install
RM ?= rm

install: ${JSONC_LIB} json_install $(KINETIC_LIB) $(KINETIC_SO_DEV)
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
	$(INSTALL) -c $(PUB_INC)/kinetic_semaphore.h $(PREFIX)/include/
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
	$(RM) -f $(PREFIX)/include/kinetic_semaphore.h
	$(RM) -f $(PREFIX)/include/byte_array.h
	$(RM) -f $(PREFIX)/include/kinetic.pb-c.h
	$(RM) -f $(PREFIX)/include/protobuf-c/protobuf-c.h
	$(RM) -f $(PREFIX)/include/protobuf-c.h
	if [ -f ${JSONC}/Makefile ]; then cd ${JSONC} && make uninstall; fi;

.PHONY: install uninstall json_install json_uninstall

#===============================================================================
# Java Simulator Support
#===============================================================================

update_simulator:
	cd vendor/kinetic-java; mvn clean package; cd -
	cp vendor/kinetic-java/kinetic-simulator/target/*.jar vendor/kinetic-java-simulator/

start_sims:
	./scripts/startSimulators.sh

start_simulator: start_sims

stop_sims:
	./scripts/stopSimulators.sh

stop_simulator: stop_sims

.PHONY: update_simulator start_sims start_simulator stop_sims stop_simulator


#===============================================================================
# Unity Test Framework Support
#===============================================================================
UNITY_INC = ./vendor/unity/src
UNITY_SRC = ./vendor/unity/src/unity.c


#===============================================================================
# System Tests
#===============================================================================

SYSTEST_SRC = ./test/system
SYSTEST_OUT = $(BIN_DIR)/systest
SYSTEST_LDFLAGS += -lm $(KINETIC_LIB) -L${OUT_DIR} -L${OPENSSL_PATH}/lib -lssl -lcrypto -lpthread -ljson-c
SYSTEST_WARN = -Wall -Wextra -Werror -Wstrict-prototypes -pedantic -Wno-missing-field-initializers -Werror=strict-prototypes
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
	$(CC) -o $@ $< $(word 2,$^) ./test/support/system_test_fixture.c ./test/support/system_test_kv_generate.c $(UNITY_SRC) $(SYSTEST_CFLAGS) $(LIB_INCS) -I$(UNITY_INC) -I./test/support $(SYSTEST_LDFLAGS) $(KINETIC_LIB)

$(SYSTEST_OUT)/%.testpass : $(SYSTEST_OUT)/run_%
	./scripts/runSystemTest.sh $*

$(systest_names) : % : $(SYSTEST_OUT)/%.testpass

system_tests: $(systest_passfiles)


#===============================================================================
# Test Utility Build Support
#===============================================================================

UTILITY = kinetic-c-util
UTIL_DIR = ./src/utility
UTIL_EXEC = $(BIN_DIR)/$(UTILITY)
UTIL_OBJ = $(OUT_DIR)/main.o
UTIL_LDFLAGS += -lm $(KINETIC_LIB) -L${OUT_DIR} -L${OPENSSL_PATH}/lib -lssl -lcrypto -lpthread -ljson-c

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


#===============================================================================
# Service Discovery Utility Build Support
#===============================================================================

DISCOVERY_UTILITY = kinetic-c-discovery
DISCOVERY_UTIL_DIR = $(UTIL_DIR)
DISCOVERY_UTIL_EXEC = $(BIN_DIR)/$(DISCOVERY_UTILITY)
DISCOVERY_UTIL_OBJ = $(OUT_DIR)/discovery.o $(OUT_DIR)/socket99.o
DISCOVERY_UTIL_LDFLAGS +=  -lm -lssl $(KINETIC_LIB) -L${OUT_DIR} -lcrypto -lpthread -ljson-c

$(OUT_DIR)/discovery.o: $(DISCOVERY_UTIL_DIR)/discovery.c
	$(CC) -c -o $@ $< $(CFLAGS) -I$(PUB_INC) -I$(DISCOVERY_UTIL_DIR) $(LIB_INCS)

$(DISCOVERY_UTIL_EXEC): $(DISCOVERY_UTIL_OBJ) $(KINETIC_LIB) $(JSONC_LIB)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Building service discovery utility: $(DISCOVERY_UTIL_EXEC)
	@echo --------------------------------------------------------------------------------
	$(CC) -o $@ $(DISCOVERY_UTIL_OBJ) $(CFLAGS) $(DISCOVERY_UTIL_LDFLAGS) $(KINETIC_LIB)

discovery_utility: $(DISCOVERY_UTIL_EXEC)

build: discovery_utility


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

run: $(UTIL_EXEC)
	@echo
	@echo --------------------------------------------------------------------------------
	@echo Running test utility: $(UTIL_EXEC)
	@echo --------------------------------------------------------------------------------
	@echo
	# exec $(UTIL_EXEC) --seterasepin --pin "" --newpin 1234
	# exec $(UTIL_EXEC) --instanterase --pin 1234
	# exec $(UTIL_EXEC) --seterasepin --pin 1234 --newpin ""
	# @echo
	exec $(UTIL_EXEC) --help
	@echo
	exec $(UTIL_EXEC) -?
	@echo
	exec $(UTIL_EXEC) --noop --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --put --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --put --key goo --value Goodbye! --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --get --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getnext --key A --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getnext --key foo --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getprevious --key zoo --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getprevious --key goo --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --delete --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getlog --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getlog --logtype utilizations --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getlog --logtype temperatures --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getlog --logtype capacities --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getlog --logtype configuration --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getlog --logtype statistics --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getlog --logtype messages --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getlog --logtype limits --host $(KINETIC_HOST1)
	@echo
	exec $(UTIL_EXEC) --getdevicespecificlog --devicelogname com.Seagate --host $(KINETIC_HOST1)
	@echo
	@echo Test Utility integration tests w/ kinetic-c lib passed!
	@echo


#===============================================================================
# Standalone Example Executables
#===============================================================================

EXAMPLE_SRC = ./src/examples
EXAMPLE_LDFLAGS += -lm -l ssl $(KINETIC_LIB) -l crypto -l pthread
EXAMPLE_CFLAGS += -Wno-deprecated-declarations
EXAMPLES = write_file_blocking

example_sources = $(wildcard $(EXAMPLE_SRC)/*.c)
example_executables = $(patsubst $(EXAMPLE_SRC)/%.c,$(BIN_DIR)/examples/%,$(example_sources))

$(BIN_DIR)/examples/%: $(EXAMPLE_SRC)/%.c $(KINETIC_LIB)
	@echo
	@echo ================================================================================
	@echo Building example: '$<'
	@echo --------------------------------------------------------------------------------
	$(CC) -o $@ $< $(CFLAGS) $(EXAMPLE_CFLAGS) -I$(PUB_INC) $(UTIL_LDFLAGS) $(KINETIC_LIB)
	@echo ================================================================================
	@echo

build_examples: $(example_executables)

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
	run_example_put_nonblocking \
	run_example_get_nonblocking \
	run_example_write_file_blocking \
	run_example_write_file_blocking_threads \
	run_example_write_file_nonblocking \
	run_example_get_key_range

valgrind_examples: setup_examples \
	valgrind_put_nonblocking \
	valgrind_get_nonblocking \
	valgrind_example_write_file_blocking \
	valgrind_example_write_file_blocking_threads \
	valgrind_example_write_file_nonblocking \
	valgrind_example_get_key_range
