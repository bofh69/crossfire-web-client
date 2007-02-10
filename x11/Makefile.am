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


bin_PROGRAMS = crossfire-client-x11

crossfire_client_x11_SOURCES = \
	png.c  \
	sound.c  \
	x11.c  \
	xutil.c


noinst_HEADERS = \
	clientbmap.h\
	x11.h \
	x11proto.h

dist_man6_MANS = \
	crossfire-client-x11.man

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/common -I$(top_srcdir)/pixmaps
crossfire_client_x11_LDADD = $(DMALLOC_LIB) $(X_LIBS) ../common/libcfclient.a -llua -llualib

../common/libcfclient.a:
	( cd ../common ; make libcfclient.a )


# Check if everything is installed as it should
installcheck-local:
	$(bindir)/crossfire-client-x11 -help

proto:
	cproto -e -o x11proto.h $(AM_CPPFLAGS) $(crossfire_client_x11_SOURCES)
	chmod 644 x11proto.h

