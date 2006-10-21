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


bin_PROGRAMS = crossfire-client-gtk

crossfire_client_gtk_SOURCES = \
	config.c \
	gx11.c \
	help.c \
	image.c \
	inventory.c \
	keys.c \
	map.c \
	png.c \
	sdl.c \
	sound.c \
	text.c

noinst_HEADERS = \
	gtkproto.h \
	gx11.h \

EXTRA_DIST = \
	crossfire-client.desktop

dist_man6_MANS = \
	crossfire-client-gtk.man

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common -I$(top_srcdir)/pixmaps $(GTK_CFLAGS) $(SDL_CFLAGS) 
crossfire_client_gtk_LDADD = $(GTK_LIBS) $(SDL_LIBS) $(DMALLOC_LIB) ../common/libcfclient.a

../common/libcfclient.a:
	( cd ../common ; make libcfclient.a )


# Check if everything is installed as it should
installcheck-local:
	$(bindir)/crossfire-client-gtk -help

proto:
	cproto -e -o gtkproto.h $(AM_CPPFLAGS) $(crossfire_client_gtk_SOURCES) 
	chmod 644 gtkproto.h

