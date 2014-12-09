SOCKET99_PATH=	../../../vendor/socket99
THREADPOOL_PATH=	../threadpool

OPT=		-O0
LIB_INC =	-I${SOCKET99_PATH} -I${THREADPOOL_PATH}
CFLAGS +=	-std=c99 ${OPT} -g ${LIB_INC}
LDFLAGS +=	-L. -lcasq -L${SOCKET99_PATH} -lsocket99 -L${THREADPOOL_PATH} -lthreadpool

BUS_OBJS = \
	bus.o \
	listener.o \
	sender.o \
	util.o \

ECHOSRV_OBJS = \
	echosrv.o \
	util.o \

all: bus.png libcasq.a test_casq echosrv bus_example

test: test_casq
	./test_casq

%.png: %.dot
	dot -Tpng -o $@ $^

libcasq.a: casq.o
	ar -rcs $@ $^

test_casq: libcasq.a test_casq.o
	${CC} -o $@ ${LDFLAGS} $^

echosrv: libcasq.a ${ECHOSRV_OBJS}
	${CC} -o $@ ${LDFLAGS} $^

bus_example: libcasq.a ${BUS_OBJS} bus_example.o
	${CC} -o $@ ${LDFLAGS} $^

clean:
	rm -f *.a *.o test_casq echosrv bus_example

tags: TAGS

TAGS: *.[ch]
	etags *.[ch]

*.o: bus_types.h
