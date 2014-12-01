AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-I$(top_builddir)/common \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/common/shared \
	-I$(top_srcdir)/help

AM_CFLAGS = \
	${GTK_CFLAGS} ${OPENGL_CFLAGS} ${SDL_CFLAGS} ${X11_CFLAGS} \
	${PTHREAD_CFLAGS}

bin_PROGRAMS = crossfire-client-gtk2

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

crossfire_client_gtk2_SOURCES = \
	account.c \
	config.c \
	create_char.c \
	gtk2proto.h \
	image.c \
	image.h \
	info.c \
	info.h \
	inventory.c \
	keys.c \
	magicmap.c \
	main.c \
	main.h \
	map.c \
	menubar.c \
	metaserver.c \
	opengl.c \
	pickup.c \
	png.c \
	sdl.c \
	skills.c \
	sound.c \
	spells.c \
	stats.c

crossfire_client_gtk2_LDADD = \
	${common_LIBS} ${PTHREAD_LIBS} ${INTLLIBS} ${WIN32_LIBS} ${LUA_LIBS} \
	${GTK_LIBS} ${OPENGL_LIBS} ${SDL_LIBS} ${X11_LIBS}
