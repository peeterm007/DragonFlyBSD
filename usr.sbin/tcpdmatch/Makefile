#
# $FreeBSD: src/usr.sbin/tcpdmatch/Makefile,v 1.2.2.2 2001/04/25 12:11:01 ru Exp $
#

PROG=	tcpdmatch
MAN=	tcpdmatch.8
SRCS=	tcpdmatch.c fakelog.c inetcf.c scaffold.c
WARNS?=	2

CFLAGS+=-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10
.if !defined(NOINET6)
CFLAGS+=-DINET6
.endif

DPADD=	${LIBWRAP}
LDADD=	-lwrap

# XXX NOSHARED libwrap.a(rfc931.o): multiple definition of 'rfc931_timeout'
.if defined(NOSHARED) && ( ${NOSHARED} != "no" && ${NOSHARED} != "NO" )
LDFLAGS+=	-Wl,--allow-multiple-definition
.endif

.PATH:	${.CURDIR}/../../contrib/tcp_wrappers

.include <bsd.prog.mk>
