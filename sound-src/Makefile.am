bin_PROGRAMS = cfsndserv

AM_CFLAGS = \
	-I$(top_builddir)/common \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/common/shared

common_BUILT_FILES = \
	$(top_builddir)/common/svnversion.h

$(common_BUILT_FILES):
	pushd `dirname $(@)` >/dev/null; \
	$(MAKE) `basename $(@)`; \
	popd >/dev/null;

BUILT_SOURCES = \
	$(common_BUILT_FILES)

common_LIBS = \
	$(top_builddir)/common/libcfclient.a

$(common_LIBS):
	pushd `dirname $(@)` >/dev/null; \
	$(MAKE); \
	popd >/dev/null;

cfsndserv_SOURCES = \
	cfsndserv.c \
	common.h \
	init.c \
	main.c \
	parser.c \
	sndproto.h

cfsndserv_LDADD = \
	$(SND_LIBS) \
	$(DMALLOC_LIB) \
	$(common_LIBS)
