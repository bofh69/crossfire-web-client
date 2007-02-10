#
# Makefile for crossfire client
#
#   Copyright (C) 2005 Mark Wedel & Crossfire Development Team
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
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(top_srcdir)/common -I$(top_srcdir)/help \
	@GTK2_CFLAGS@ \
	@SDL_CFLAGS@

bin_PROGRAMS = crossfire-client-gtk2

PROTO_SOURCES = \
        about.c \
	config.c \
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
	sound.c \
	spells.c \
	stats.c

crossfire_client_gtk2_SOURCES = \
	$(PROTO_SOURCES) \
	support.c support.h \
	interface.c interface.h \
	image.h main.h gtk2proto.h \
	callbacks.h 

crossfire_client_gtk2_LDADD = ../../common/libcfclient.a @GTK2_LIBS@ @DMALLOC_LIB@ $(INTLLIBS) $(X_LIBS) @SDL_LIBS@ @OPENGL_LIBS@ @WIN32_LIBS@ -llua -llualib

proto:
	cproto -e -o gtk2proto.h  $(SDL_CFLAGS) $(GTK2_CFLAGS) $(DEFAULT_INCLUDES) $(PROTO_SOURCES)
	chmod 644 gtk2proto.h

