## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I../../common \
	@PACKAGE_CFLAGS@ \
	@SDL_CFLAGS@

bin_PROGRAMS = gcfclient2

PROTO_SOURCES = \
	config.c \
	image.c \
	info.c \
	inventory.c \
	keys.c \
	main.c \
	map.c \
	magicmap.c \
	menubar.c \
	opengl.c \
	png.c \
	sdl.c \
	sound.c \
	stats.c

gcfclient2_SOURCES = \
	$(PROTO_SOURCES) \
	support.c support.h \
	interface.c interface.h 


gcfclient2_LDADD = @PACKAGE_LIBS@ @DMALLOC_LIB@ $(INTLLIBS) ../../common/libcfclient.a @SDL_LIBS@ @OPENGL_LIBS@


proto:
	cproto -e -o gtk2proto.h  $(SDL_CFLAGS) $(PACKAGE_CFLAGS) $(DEFAULT_INCLUDES) $(PROTO_SOURCES)
	chmod 644 gtk2proto.h
