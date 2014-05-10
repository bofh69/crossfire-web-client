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
	svnversion.h \
	version.h

BUILT_SOURCES = svnversion.h
CLEANFILES = svnversion.h

EXTRA_DIST = \
	def-keys \
	item-types \
	items.pl

AM_CFLAGS = $(PTHREAD_CFLAGS) @LUA_CFLAGS@

proto:
	cproto -DCPROTO -DCFCLIENT $(DEFAULT_INCLUDES) $(AM_CFLAGS) \
		$(libcfclient_a_SOURCES) > proto.h
	chmod 644 proto.h

item-types.h: item-types
	$(PERL) $(srcdir)/items.pl $(srcdir)

def-keys.h: def-keys
	$(srcdir)/mdk.sh $(srcdir)

msgtypes.h: $(srcdir)/shared/newclient.h
	$(PERL) $(srcdir)/msgtypes.pl $(srcdir)

svnversion.h:
	OUTPUT_DATA='/* Auto-generated at build time. */'; \
	if [ "X`echo -n`" = "X-n" ]; then \
		echo_n() { echo $${1+"$$@"}"\c"; } \
	else \
		echo_n() { echo -n $${1+"$$@"}; } \
	fi; \
	if test -n "$(SVNVERSION)" -a -d "$(top_srcdir)/.svn"; \
	then OUTPUT_DATA=`echo "$$OUTPUT_DATA"; echo_n '#define SVN_REV "'; $(SVNVERSION) -n "$(top_srcdir)"`'"'; \
	fi; \
	if test ! -e svnversion.h; then \
		echo "$$OUTPUT_DATA" > svnversion.h; \
	elif test "$$OUTPUT_DATA" != "`cat svnversion.h`"; then \
		echo "$$OUTPUT_DATA" > svnversion.h; \
	fi
