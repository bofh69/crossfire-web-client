#
# Makefile for crossfire client
#
#   Copyright (C) 2005, 2010 Mark Wedel & Crossfire Development Team
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

bin_PROGRAMS = cfsndserv @ALSA9_TARGET@

EXTRA_PROGRAMS = cfsndserv_alsa9

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

cfsndserv_alsa9_SOURCES = \
	common.h \
	sndproto.h \
	def_sounds.h \
	def_sounds.c \
	common.c \
	alsa9.c

cfsndserv_alsa9_LDADD = \
	$(ALSA9_LIBS) \
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

