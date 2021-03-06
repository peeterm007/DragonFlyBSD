RELUP=	/../..
.include "../../../Makefile.inc"
SRCDIR=	${GCCDIR}/libstdc++-v3
.include "Makefile.src"

LIB=			cxx98convenience
INTERNALLIB=		yes
INTERNALLIBPROF=	yes
INSTALL_PIC_ARCHIVE=	yes

.PATH:	${SRCDIR}
.PATH:	${SRCDIR}/src
.PATH:	${SRCDIR}/src/c++98
.PATH:	${SRCDIR}/config/locale/dragonfly
.PATH:	${SRCDIR}/config/locale/generic

# for now do not let the use of -flto (-Wodr...)
.if ${CFLAGS:M-flto}
CFLAGS+=	-fno-lto
LDFLAGS+=	-flto
.endif

CXXFLAGS+=	-fdiagnostics-show-location=once
CXXFLAGS+=	-ffunction-sections -fdata-sections
SHARED_CXXFLAGS=	-D_GLIBCXX_SHARED

SRCS=	${libc__98convenience_la_SOURCES}
COWD=	collate.x monetary.x numeric.x
COWG=	messages.x
GENCC=	atomicity.cc basic_file.cc c++locale.cc
GENCC+=	${COWD:.x=_members_cow.cc} ${COWG:.x=_members_cow.cc}

FLAGS_GROUPS=		deprecated gnu11 cow concept explicit
deprecated_FLAGS=	-std=gnu++98 -D_BACKWARD_BACKWARD_WARNING_H
deprecated_FLAGS_FILES=	strstream.cc
gnu11_FLAGS=		-std=gnu++11 -fno-implicit-templates
gnu11_FLAGS_FILES=	locale_init.cc localename.cc
cow_FLAGS=		-D_GLIBCXX_USE_CXX11_ABI=0 -fimplicit-templates
cow_FLAGS_FILES=	${cow_string_host_sources}
concept_FLAGS=		-D_GLIBCXX_CONCEPT_CHECKS -fimplicit-templates
concept_FLAGS_FILES=	concept-inst.cc
parallel_FLAGS=		-D_GLIBCXX_PARALLEL -fno-implicit-templates
parallel_FLAGS_FILES=	parallel_settings.cc
explicit_FLAGS=		-fno-implicit-templates

REFERENCED_FILES=	${deprecated_FLAGS_FILES} \
			${gnu11_FLAGS_FILES} ${cow_FLAGS_FILES} \
			${concept_FLAGS_FILES} ${parallel_FLAGS_FILES}

.for F in ${SRCS}
. if ! ${REFERENCED_FILES:M${F}}
explicit_FLAGS_FILES+=	${F}
. endif
.endfor

.for F in ${COWG}
${F:.x=_members_cow.cc}: ${SRCDIR}/config/locale/generic/${F:.x=_members.cc}
	cp ${.ALLSRC} ${.TARGET}
.endfor

.for F in ${COWD}
${F:.x=_members_cow.cc}: ${SRCDIR}/config/locale/dragonfly/${F:.x=_members.cc}
	cp ${.ALLSRC} ${.TARGET}
.endfor

atomicity.cc: ${SRCDIR}/config/cpu/generic/atomicity_builtins/atomicity.h
	cp ${.ALLSRC} ${.TARGET}

basic_file.cc: ${SRCDIR}/config/io/basic_file_stdio.cc
	cp ${.ALLSRC} ${.TARGET}

c++locale.cc: ${SRCDIR}/config/locale/dragonfly/c_locale.cc
	cp ${.ALLSRC} ${.TARGET}

CLEANFILES+=	${GENCC}

beforedepend:	${GENCC}

.include <bsd.lib.mk>
