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


bin_PROGRAMS = cfsndserv @ALSA9_TARGET@
EXTRA_PROGRAMS = cfsndserv_alsa9

EXTRA_DIST = common.c sounds.dist soundsdef.h

AM_CPPFLAGS = -I$(top_srcdir)/common

noinst_HEADERS = \
	soundsdef.h

cfsndserv_SOURCES = cfsndserv.c
cfsndserv_LDADD = $(SND_LIBS) $(DMALLOC_LIB)

cfsndserv_alsa9_SOURCES = alsa9.c
cfsndserv_alsa9_LDADD =  $(ALSA9_LIBS) $(DMALLOC_LIB)


sounds: sounds.dist Makefile
	$(PERL) -p -e s#/usr/local/lib/sounds#$(SOUNDDIR)# $(srcdir)/sounds.dist > sounds

soundsdef.h: sounds
	$(PERL) $(top_srcdir)/utils/deftoheader.pl sounds soundsdef.h def_sounds

cfsndserv.o: soundsdef.h

alsa9.o: soundsdef.h
proto:
	echo "Nothing to do for proto"
