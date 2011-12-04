#
# Makefile for crossfire client
#
#   Copyright (C) 2005-2010 Mark Wedel & Crossfire Development Team
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#   The author can be reached via e-mail to crossfire-devel@real-time.com

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-I$(top_builddir)/common \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/common/shared \
	-I$(top_srcdir)/help

AM_CFLAGS = \
	$(PTHREAD_CFLAGS) \
	@LIBGLADE_CFLAGS@ \
	@SDL_CFLAGS@

bin_PROGRAMS = crossfire-client-gtk2

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

proto_FILES = \
	about.c \
	account.c \
	config.c \
	create_char.c \
	image.c \
	info.c \
	inventory.c \
	keys.c \
	main.c \
	map.c \
	magicmap.c \
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

crossfire_client_gtk2_SOURCES = \
	image.h \
	info.h \
	main.h \
	gtk2proto.h \
	$(proto_FILES)

crossfire_client_gtk2_LDADD = $(common_LIBS) $(PTHREAD_LIBS) @LIBGLADE_LIBS@ @DMALLOC_LIB@ $(INTLLIBS) $(X_LIBS) @SDL_LIBS@ @OPENGL_LIBS@ @WIN32_LIBS@ @LUA_LIBS@

# This target breaks when it sees -pthread in the cproto command, so it has to
# be stripped out to work.  $(filter-out) is a GNU make extension, so it is
# commented out here for portability reasons.  Is there a good way to do this?
# Removing PTHREAD_CFLAGS from here used to work, but something put it back.
#
proto:  PROTO_DIRTY = \
	$(INCLUDES) \
	$(SDL_CFLAGS) \
	$(LIBGLADE_CFLAGS) \
	$(PROTO_FILES)

# proto:  PROTO_CLEAN = $(filter-out -pthread,$(PROTO_DIRTY))
# proto:
# 	cproto -e -o gtk2proto.h $(PROTO_CLEAN); \
# 	chmod 644 gtk2proto.h

proto:
	cproto -e -o gtk2proto.h $(PROTO_DIRTY); \
 	chmod 644 gtk2proto.h

