# $FreeBSD: src/usr.sbin/ndiscvt/Makefile,v 1.7 2005/04/24 20:21:22 wpaul Exp $

.PATH:	${.CURDIR}/../../sys/emulation/ndis

PROG=	ndiscvt
SRCS=	ndiscvt.c
SRCS+=	subr_pe.c
SRCS+=	inf.c inf-token.l inf-parse.y y.tab.h

MAN=	ndiscvt.8
MAN+=	ndisgen.8

DPADD=  ${LIBL}
LDADD=  -ll

YFLAGS+=-v

CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../../sys

CLEANFILES= y.output

.if !defined(NO_SHARE)
FILES= windrv_stub.c
FILESNAME= windrv_stub.c
FILESDIR= /usr/share/misc
.endif

SCRIPTS= ndisgen.sh

.include <bsd.prog.mk>
