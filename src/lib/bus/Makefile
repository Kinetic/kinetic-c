SOCKET99_PATH=		../../../vendor/socket99
THREADPOOL_PATH=	../threadpool
LIB_PATH=		../../../obj

OPT=		-O3
LIB_INC =	-I${SOCKET99_PATH} -I${THREADPOOL_PATH} -I${OPENSSL_PATH}/include
CFLAGS +=	-std=c99 ${OPT} -Wall -g ${LIB_INC}
LDFLAGS +=	-L. -lsocket99 -L${LIB_PATH} -L${THREADPOOL_PATH} -L${OPENSSL_PATH}/lib -lssl -lcrypto

BUS_OBJS = \
	bus.o \
	bus_ssl.o \
	casq.o \
	listener.o \
	sender.o \
	util.o \
	yacht.o \

ECHOSRV_OBJS = \
	echosrv.o \
	util.o \

all: bus.png test_casq test_yacht echosrv bus_example

test: test_casq test_yacht
	./test_casq
	./test_yacht

%.png: %.dot
	dot -Tpng -o $@ $^

libbus.a: ${BUS_OBJS}
	ar -rcs $@ $^

test_casq: test_casq.o casq.o
	${CC} -o $@ $^ ${LDFLAGS}

test_yacht: test_yacht.o yacht.o
	${CC} -o $@ $^ ${LDFLAGS}

echosrv: ${ECHOSRV_OBJS}
	${CC} -o $@ $^ ${LDFLAGS}

bus_example: bus_example.o libbus.a
	${CC} -o $@ $^ ${LDFLAGS} -lbus -lthreadpool

clean:
	rm -f *.a *.o test_casq echosrv bus_example

tags: TAGS

TAGS: *.[ch]
	etags *.[ch]

*.o: bus_types.h bus_internal_types.h Makefile
sender.o: sender_internal.h
listener.o: listener_internal.h
%.o: %.h
