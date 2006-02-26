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
	-I$(top_srcdir)/common \
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
	metaserver.c \
	opengl.c \
	pickup.c \
	png.c \
	sdl.c \
	sound.c \
	spells.c \
	stats.c

gcfclient2_SOURCES = \
	$(PROTO_SOURCES) \
	support.c support.h \
	interface.c interface.h \
	image.h main.h gtk2proto.h \
	callbacks.h 

gcfclient2_LDADD = @PACKAGE_LIBS@ @DMALLOC_LIB@ $(X_LIBS) $(INTLLIBS) ../../common/libcfclient.a @SDL_LIBS@ @OPENGL_LIBS@

proto:
	cproto -e -o gtk2proto.h  $(SDL_CFLAGS) $(PACKAGE_CFLAGS) $(DEFAULT_INCLUDES) $(PROTO_SOURCES)
	chmod 644 gtk2proto.h

