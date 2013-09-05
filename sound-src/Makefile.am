bin_PROGRAMS = cfsndserv

EXTRA_DIST = \
	sounds

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
	$(common_BUILT_FILES) \
	def_sounds.h \
	def_sounds.c

common_LIBS = \
	$(top_builddir)/common/libcfclient.a

$(common_LIBS):
	pushd `dirname $(@)` >/dev/null; \
	$(MAKE); \
	popd >/dev/null;

cfsndserv_SOURCES = \
	common.h \
	sndproto.h \
	def_sounds.h \
	def_sounds.c \
	common.c \
	cfsndserv.c

cfsndserv_LDADD = \
	$(SND_LIBS) \
	$(DMALLOC_LIB) \
	$(common_LIBS)

def_sounds.h: sounds
	$(PERL) \
	    $(top_srcdir)/utils/deftoheader.pl $(srcdir)/sounds def_sounds.h def_sounds

def_sounds.c: sounds
	$(PERL) \
	    $(top_srcdir)/utils/deftoheader.pl $(srcdir)/sounds def_sounds.c def_sounds

proto_FILES = \
	common.c

proto:
	cproto -e -o sndproto.h \
	    $(DEFAULT_INCLUDES) $(AM_CFLAGS) $(proto_FILES)
	chmod 644 sndproto.h
