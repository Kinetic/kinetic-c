SOCKET99_PATH=		../../../vendor/socket99
THREADPOOL_PATH=	../threadpool
LIB_PATH=		../../../obj
BASE_PROJECT_INC=	-I.. -I../../../include -I../../../vendor/protobuf-c

OPT=		-O3
LIB_INC =	-I${SOCKET99_PATH} -I${THREADPOOL_PATH} -I${OPENSSL_PATH}/include
CFLAGS +=	-std=c99 ${OPT} -Wall -g ${LIB_INC} ${BASE_PROJECT_INC}
LDFLAGS +=	-L. -L${LIB_PATH} -L${THREADPOOL_PATH} -L${OPENSSL_PATH}/lib -L${SOCKET99_PATH} -lsocket99 -lssl -lcrypto

BUS_OBJS = \
	bus.o \
	bus_poll.o \
	bus_ssl.o \
	listener.o \
	listener_cmd.o \
	listener_helper.o \
	listener_io.o \
	listener_task.o \
	send.o \
	send_helper.o \
	syscall.o \
	util.o \
	yacht.o \

ECHOSRV_OBJS = \
	echosrv.o \
	util.o \

all: bus.png echosrv bus_example

%.png: %.dot
	dot -Tpng -o $@ $^

libbus.a: ${BUS_OBJS}
	ar -rcs $@ $^

echosrv: ${ECHOSRV_OBJS}
	${CC} -o $@ $^ ${LDFLAGS}

bus_example: bus_example.o libbus.a
	${CC} -o $@ $^ ${LDFLAGS} -lbus -lthreadpool

clean:
	rm -f *.a *.o echosrv bus_example

tags: TAGS

TAGS: *.[ch]
	etags *.[ch]

*.o: bus_types.h bus_internal_types.h Makefile
sender.o: sender_internal.h
listener.o: listener_internal.h
%.o: %.h
