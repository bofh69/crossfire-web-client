AM_CFLAGS = ${PTHREAD_CFLAGS} ${LUA_CFLAGS} ${GLIB_CFLAGS}

noinst_LIBRARIES = libcfclient.a

libcfclient_a_SOURCES = \
	client.c \
	commands.c \
	image.c \
	init.c \
	item.c \
	mapdata.c \
	metaserver.c \
	misc.c \
	newsocket.c \
	p_cmd.c \
	player.c \
	script.c \
	script_lua.c

noinst_HEADERS = \
	client-types.h \
	client.h \
	config.h \
	def-keys.h \
	external.h \
	item-types.h \
	item.h \
	mapdata.h \
	metaserver.h \
	msgtypes.h \
	p_cmd.h \
	proto.h \
	script.h \
	shared/newclient.h \
	version.h

EXTRA_DIST = \
	def-keys \
	item-types \
	items.pl

item-types.h: item-types
	$(PERL) $(srcdir)/items.pl $(srcdir)

def-keys.h: def-keys
	$(PERL) $(srcdir)/mdk.pl $(srcdir)

msgtypes.h: $(srcdir)/shared/newclient.h
	$(PERL) $(srcdir)/msgtypes.pl $(srcdir)
