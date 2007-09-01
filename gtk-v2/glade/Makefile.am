#
# Makefile for crossfire 0.95.0 and beyond
#
# This file will eventually be replaced by a configure script.  But for
# now, this simple makefile should do the job.
#
#   Copyright (C) 2001 Mark Wedel & Crossfire Development Team
#   Copyright (C) 1992 Frank Tore Johansen
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
#   The authors can be reached via e-mail to crossfire-devel@real-time.com

XML_FILES = \
	caelestis.glade \
	chthonic.glade \
	dialogs.glade \
	gtk-v1.glade \
	gtk-v2.glade \
 	meflin.glade

PRJ_FILES = $(foreach XML,$(XML_FILES),$(XML)p)

gladedir=$(pkgdatadir)/glade-gtk2

glade_DATA = $(XML_FILES)

EXTRA_DIST = \
	$(XML_FILES) \
	$(PRJ_FILES) \
	README

