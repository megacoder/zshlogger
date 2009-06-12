CCMODE	=32
#CCMODE	=64
CC	=gcc -m${CCMODE}
#CFLAGS	=-Os -Wall -Werror -pedantic -g
CFLAGS	=-O0 -Wall -Werror -pedantic -g
LDFLAGS	=-g
LDLIBS	=-lreadline -lncurses

PREFIX	=/opt
BINDIR	=${PREFIX}/bin

all:	zshlogger

clean:
	${RM} *.o a.out core.* lint tags

distclean clobber: clean
	${RM} zshlogger

tags:	*.c
	ctags -R .

install: zshlogger
	install -D -s zshlogger ${BINDIR}/zshlogger

uninstall:
	${RM} ${BINDIR}/zshlogger

check:	zshlogger test
	./zshlogger -f test ${ARGS}
