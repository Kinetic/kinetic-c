PROJECT =	threadpool
OPTIMIZE =	-O3
WARN =		-Wall -pedantic #-Wextra
#CDEFS +=
#CINCS +=
CSTD +=		-std=c99 #-D_POSIX_C_SOURCE=1 -D_C99_SOURCE

CFLAGS +=	${CSTD} -g ${WARN} ${CDEFS} ${CINCS} ${OPTIMIZE}
LDFLAGS +=	-lpthread

TEST_CFLAGS = 	${CFLAGS}
TEST_LDFLAGS += ${LDFLAGS} -L. -lthreadpool

all: test_${PROJECT}
all: test_${PROJECT}_stress
all: test_${PROJECT}_sequencing
all: lib${PROJECT}.a

OBJS=		threadpool.o

TEST_OBJS=

# Basic targets

lib${PROJECT}.a: ${OBJS}
	ar -rcs lib${PROJECT}.a ${OBJS}

test_${PROJECT}: test_${PROJECT}.o ${TEST_OBJS} lib${PROJECT}.a
	${CC} -o $@ $^ ${TEST_CFLAGS} ${TEST_LDFLAGS}

test_${PROJECT}_%: test_${PROJECT}_%.o ${TEST_OBJS} lib${PROJECT}.a
	${CC} -o $@ $^ ${TEST_CFLAGS} ${TEST_LDFLAGS}

test: lib${PROJECT}.a ./test_${PROJECT}
	./test_${PROJECT}

clean:
	rm -f ${PROJECT} test_${PROJECT} test_${PROJECT}_stress *.o *.a *.core

# Installation
PREFIX ?=	/usr/local
INSTALL ?=	install
RM ?=		rm

install:
	${INSTALL} -c ${PROJECT} ${PREFIX}/bin

uninstall:
	${RM} -f ${PREFIX}/bin/${PROJECT}
