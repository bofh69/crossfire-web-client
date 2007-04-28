#
# Grab the crossfire-images archive of the sourceforge files list.  If
# you have a copy of the arch directory, you can run the
# adm/collect_images -archive from the lib directory of the server and
# it will make the archive.
#
# Now maintaining this - easy enough to do if it proves useful.
# MSW 2005-02-28
#
%define Name crossfire
%define extra client
%define version 2.0-dev
%define sndversion 2.0-dev
%define release 1
%define prefix /usr/X11R6
%define _sourcedir /export/home/crossfire/Crossfire
%define _srcrpmdir /export/home/crossfire/RPM-SRC
%define _rpmdir /export/home/crossfire/RPMS
%define _topdir /export/home/crossfire/RPM-TOP

Name: %{Name}-%{extra}
Version: %{version}
Release: 1
Summary: Client for connecting to crossfire servers.
Group: Amusements/Games/Crossfire
License: GPL
URL: http://crossfire.real-time.com
Source0: %{name}-%{version}.tar.gz
Source1: %{name}-sounds-%{version}.tar.gz
Source2: %{name}-images-%{version}.tar.gz
Provides: crossfire-client
Requires: SDL
Requires: SDL_image
Requires: alsa-lib
BuildRequires: SDL-devel
BuildRequires: SDL_image-devel
BuildRequires: alsa-lib-devel
Epoch: 4
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
Crossfire is a highly graphical role-playing adventure game with
characteristics reminiscent of rogue, nethack, omega, and gauntlet. 
It has multiplayer capability and presently runs under X11.

Client for playing the new client/server based version of Crossfire.
This package allows you to connect to crossfire servers around the world.
You do not need install the crossfire program in order to use this
package.

%package gtk2
Summary: GTKv2 client for %{Name}
Group: X11/Games
Provides: crossfire-client

%description gtk2
GTKv2 version of the crossfire client - this is a completely new client
compared to the gtkv1 client.

%package sounds
Summary: Sound effects for the crossfire game
Group: X11/Games
Requires: crossfire-client
  
%description sounds
Sound effects for people who want sounds with their game

%package gtk
Summary: GTK client for %{Name}
Group: X11/Games
Provides: crossfire-client

%description gtk
GTK version of the crossfire client

%package common
Summary: Common files for %{Name}
Group: X11/Games
Provides: crossfire-client

%description common
File includes sounds and images.

%prep
%setup -q -a 1 -a 2 -n %{Name}-client-%{version}

%build
chmod 755 configure
%configure --datadir=/usr/share/games/crossfire \
	--with-sound-dir=/usr/share/sounds/crossfire --disable-dmalloc --with-loglevel=3

make %{?_smp_mflags}

%install
[ -n "%{buildroot}" -a "%{buildroot}" != / ] && rm -rf %{buildroot}
#
# Sounds
#
install -d %{buildroot}%{_datadir}/sounds/crossfire
install sounds/*.raw %{buildroot}%{_datadir}/sounds/crossfire
#
# Client images cd lib; adm/collect_images -archive
#
install -d %{buildroot}%{_datadir}/games/crossfire/%{name}
install crossfire.clsc %{buildroot}%{_datadir}/games/crossfire/%{name}
install crossfire.base %{buildroot}%{_datadir}/games/crossfire/%{name}
install bmaps.client %{buildroot}%{_datadir}/games/crossfire/%{name}
install README %{buildroot}%{_datadir}/games/crossfire/%{name}
#
# KDE
#
install -d %{buildroot}%{_datadir}/applnk/Games/Adventure
install -d %{buildroot}%{_datadir}/icons/hicolor/16x16/apps
install -d %{buildroot}%{_datadir}/icons/hicolor/32x32/apps
install -d %{buildroot}%{_datadir}/icons/hicolor/48x48/apps
install -d %{buildroot}%{_datadir}/icons/locolor/16x16/apps
install -d %{buildroot}%{_datadir}/icons/locolor/32x32/apps
install -d %{buildroot}%{_datadir}/icons/locolor/48x48/apps

#%{__make} install \
#    DESTDIR=%{buildroot} \
#    bindir=%{buildroot}%{_bindir} \
#    mandir=%{buildroot}%{_mandir}/man6

%makeinstall mandir=%{buildroot}%{_mandir}

#
# KDE
#
install -m 644 -c gtk/crossfire-client.desktop \
	%{buildroot}%{_datadir}/applnk/Games/Adventure/crossfire.desktop
install -m 644 pixmaps/16x16.png \
	%{buildroot}%{_datadir}/icons/hicolor/16x16/apps/crossfire-client.png
install -m 644 pixmaps/32x32.png \
	%{buildroot}%{_datadir}/icons/hicolor/32x32/apps/crossfire-client.png
install -m 644 pixmaps/48x48.png \
	%{buildroot}%{_datadir}/icons/hicolor/48x48/apps/crossfire-client.png
install -m 644 pixmaps/16x16.png \
	%{buildroot}%{_datadir}/icons/locolor/16x16/apps/crossfire-client.png
install -m 644 pixmaps/32x32.png \
	%{buildroot}%{_datadir}/icons/locolor/32x32/apps/crossfire-client.png
install -m 644 pixmaps/48x48.png \
	%{buildroot}%{_datadir}/icons/locolor/48x48/apps/crossfire-client.png


%post
rm -f %{_datadir}/gnome/apps/Games/crossfire.desktop
rm -f %{_datadir}/gnome/ximian/Programs/Games/crossfire.desktop

%clean
[ -n "%{buildroot}" -a "%{buildroot}" != / ] && rm -rf %{buildroot}

%files
%defattr(644,root,root,755)
%doc ChangeLog COPYING License NOTES README TODO
%attr(755,root,root) %{_bindir}/cfclient
%{_mandir}/man6/cfclient.6*

%files gtk
%defattr(644,root,root,755)
%doc ChangeLog COPYING License NOTES README TODO
%attr(755,root,root) %{_bindir}/gcfclient
%{_mandir}/man6/gcfclient.6*

%files gtk2
%defattr(644,root,root,755)
%doc ChangeLog COPYING License NOTES README TODO
%attr(755,root,root) %{_bindir}/gcfclient2


%files common
%defattr(644,root,root,755)

# Image data
%{_datadir}/games/crossfire/crossfire-client/README
%{_datadir}/games/crossfire/crossfire-client/bmaps.client
%{_datadir}/games/crossfire/crossfire-client/crossfire.base
%{_datadir}/games/crossfire/crossfire-client/crossfire.clsc
#
# KDE
#
%{_datadir}/applnk/Games/Adventure/*.desktop
%{_datadir}/icons/hicolor/16x16/apps/%{name}.png
%{_datadir}/icons/hicolor/32x32/apps/%{name}.png
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/icons/locolor/16x16/apps/%{name}.png
%{_datadir}/icons/locolor/32x32/apps/%{name}.png
%{_datadir}/icons/locolor/48x48/apps/%{name}.png

# Not supported yet
#%files gnome
#%defattr(644,root,root,755)
#%doc ChangeLog COPYING License NOTES README TODO
#%attr(755,root,root) /usr/X11R6/bin/gnome-cfclient
#/usr/X11R6/man/man6/gnome-cfclient.6*
#/usr/share/gnome/apps/Games/Tclug/crossfire.desktop
#/usr/share/pixmaps/shield.png

%files sounds
%defattr(644,root,root,755)
%dir %{_datadir}/sounds/crossfire
%attr(444,root,root) %{_datadir}/sounds/crossfire/*
%attr(755,root,root) %{_bindir}/cfsndserv
%attr(755,root,root) %{_bindir}/cfsndserv_alsa9



%changelog
* Wed Jun 28 2006 Mark Wedel <mwedel@sonic.net>
+ crossfire-client-1.9.1-1
- new release 1.9.1

* Sun Feb 26 2006 Mark Wedel <mwedel@sonic.net>
+ crossfire-client-1.9.0-1
- new release 1.9.0

* Mon Feb 28 2005 Mark Wedel <mwedel@sonic.net>
+ crossfire-client-1.7.1-1
- new release 1.7.1

* Wed Feb 26 2003 Bob Tanner <tanner@real-time.com>
+ crossfire-client-1.5.0-1.realtime
- new release 1.5.0

* Wed Feb 20 2003 Bob Tanner <tanner@real-time.com>
+ crossfire-client-20030220CVS-1.realtime
- MSW: Fix bug in rescale_rgba_data() that was potentially causing a 1 byte
  overrun of malloc'd data, that could result in crashes or other odd problems.

* Wed Feb 19 2003 Bob Tanner <tanner@real-time.com>
+ crossfire-client-20030219CVS-1.realtime
- upgrade to cvs snapshot from 02/19/2003
- reworked configure and build to take advantage the new autoconf stuff

* Sat Sep 28 2002 Bob Tanner <tanner@real-time.com>
  + crossfire-client-1.4.0-realtime.1
  - upgrade to 1.4.0
  - http://sourceforge.net/project/shownotes.php?group_id=13833&release_id=110812

* Wed Jul 25 2002 Bob Tanner <tanner@real-time.com>
  + crossfire-client-1.3.1-realtime.4
  - fixed crossfire-client.desktop entry
  - fix for init_SDL bug
  - added Requires: SDL, SDL_image
  - added BuildRequires: SDL-devel, SDL_image-devel

* Wed Jul 10 2002 Bob Tanner <tanner@real-time.com>
  + crossfire-client-1.3.1-realtime.3
  - fixed location of sound files [kbulgrien@worldnet.att.net]

* Wed Jul 02 2002 Bob Tanner <tanner@real-time.com>
  + crossfire-client-1.3.1-realtime.2
  - added 16x16, 32x32, 48x48 icons for proper KDE support
  - added support for SMP builds

* Wed Jul 02 2002 Bob Tanner <tanner@real-time.com>
  + crossfire-client-1.3.1-realtime.1
  - released 1.3.1 client
  - BUG Fix 
    http://mailman.real-time.com/pipermail/crossfire-devel/2002-July/003273.html     - Enhancement
    http://www.geocrawler.com/lists/3/SourceForge/7318/0/9103079/
    http://www.geocrawler.com/lists/3/SourceForge/7318/0/9093313/

* Wed Jul 02 2002 Bob Tanner <tanner@real-time.com>
  + crossfire-client-1.3.0-realtime.1
  - released 1.3.0 client
  - CHANGELOG 
    http://mailman.real-time.com/pipermail/crossfire-list/2002-July/000943.html

* Mon May 06 2002 Bob Tanner <tanner@real-time.com>
  + crossfire-client-20020424-realtime.5
  - missing some files dealing with cache images

* Tue Apr 30 2002 Bob Tanner <tanner@real-time.com>
  + crossfire-client-20020424-realtime.4
  - moved desktop entries to the tclug sub-menu

* Wed Apr 24 2002 Bob Tanner <tanner@real-time.com>
  + crossfire-client-20020424-realtime.2
  - change hard coded commands to rpms macros
  - change several file locations to comply with LSB 
  - add crossfire-client to tclug-gampak; An apt4redhat virtual package
  - tclug-gamepak via apt rpm ftp://ftp.real-time.com/linux/apt realtime/7.2/i386 tclug

* Thu Feb 14 2002 Bob Tanner <tanner@real-time.com>
- configure.in, configure: Add check for zlib before png lib check, as on
  some systems, png requires -lz.
- common/client-types.h: Add #ifdef check for SOL_TCP
- common/client.c: Add fast_tcp_send variable, comment out printing of error
  from socket EOF.  Use TCP_NODELAY for sending data to the server
  if TCP_NODELAY is available.  cs_write_string modified to use
  cs_print_string.
- common/client.h: Remove display_mode enum, add fast_tcp_send extern.
- common/commands.c, common/init.c,gtk/image.c, gtk/map.c
  cs_write_sting modified to use cs_print_string
- common/external.h: set_autorepeat extern added.
- common/newsocket.c: Modified to be better optimized for using TCP_NODELAY -
  cs_print_string function added.
- common/player.c: modified to use cs_print_string , autorepeat client side
  command added.
- common/proto.h, gtk/gtkproto.h: updated with new functions
- gnome/gnome.c: display_mode variable removed, cs_write_string
  replaced with cs_print_string
- gtk/gx11.c: display_mode variable removed, cs_write_string replaced with
  cs_print_string, -nofog option added
- pixmaps/question.111: Resized to be 32x32
  pixmaps/*.xbm - used for inventory icons in X11 client, replacing xpm
  files
- sound-src/cfsndserv.c: Better error handling, include time.h
- x11/cfclient.man: -font and -noautorepeat options added.
- x11/png.c: better error checking for rescaling images
- x11/x11.c: noautorepeat variable added, display_mode removed, image icon
  functionality re-enabled, images now created from xbm files,
  set_autorepeat function added, add ability to set font, add mouse
  wheel support
- x11/x11.h: remove screen_num extern.
- x11/x11proto.h: Updated with new functions.
- x11/xutil.c: Modified to use image_size instead of hardcoded 24x24 value
  for the status icons.  cs_write_replaced with cs_print_string, no
  auto repeat functionality added.

* Mon Dec 31 2001 Bob Tanner <tanner@real-time.com>
- Rolled 1.1.0 client
- NOTE Mark's new email address
- Fixed typo in install target for x11 client.
- Make all clients Provide: crossfire-client
- Make sounds dependent on crossfire-client.

* Sun Dec 30 2001 Mark Wedel <mwedel@sonic.net>
- README: Update notes on needing png (and not xpm) library.  Update mailing 
  alias.
- configure.in, configure: As the seperate sound program (cfsndserv) is the
  only supported sound configuration, remove new_sound_system defines
  and ability to use the old (now non existant) sound system.
  Have configure exit with error message if png library is not found, 
  as it is critical to the build process.  Change it so that
  gnome/Makefile is always built so that making of releases works.
- gnome/gnome-cfclient.man, help/about.h, x11/cfclient.man: Update mail address.
- gtk/gtkproto.h, x11/x11proto.h: Rebuilt, prototypes for some changed for
  signed to unsigned characters.
- gtk/gx11.c, gtk/png.c, pixmaps/stipple.111, x11/png.c, x11/x11.c, x11/xutil.c,
  pixmaps/stipple.111 pixmaps/stipple.112:
  Mostly changes to fix compile warnings and make sure we are passing the
  right types to the various image creation functions (8 bit data).
- sound-src/Makefile.in: Add soundsdef.h to list of things to build.
- x11/x11.h: Remove extra semicolon.

* Mon May 14 2001 Bob Tanner <tanner@real-time.com>
- Rolled new client 1.0.0

* Sun May 13 2001 Mark Wedel <mwedel@scruz.net>
- player.c: Fix for client crashes if player enters really long commands (like
   say .....).
- gx11.c,command.c: Remove some debug statements which really should not be
  there for 1.0, and which are not really useful anyways.  items_types,
  item_types.h: Varioius minor updates.
- gx11.c: Fix bug that causes gtk client not to update weapon speed.
  metaserver.c: Have the listing get sorted by hostname to make it easier to
  find the host the user may want.

* Wed Apr 11 2001 Bob Tanner <tanner@real-time.com>
- Rolled new client 0.98.0 with Mark's changes listed next

* Tue Apr 10 2001 Mark Wedel <mwedel@scruz.net>
- Change matching for sword - hopefully this should fix problems with dancing
  sword spellbooks.
- Move animations of the look window to the client.  All the necessary was
  already being sent to the client - it was just needed for the client to use 
  this information.  Also remove some 
- Only resort items based on name if the name has changed.  This fixes a problem
  with items moving around in the inventory if you lock/apply/unapply/unlock 
  them. 

* Wed Mar 21 2001 Bob Tanner <tanner@real-time.com>
- Rolled new client 0.97.0 with Mark's changes listed next

* Tue Mar 20 2001 Mark Wedel <mwedel@scruz.net>
- Change so that containers on the ground still keep proper contents even if the
  map space itself changes (spells or other objects going onto the space). 
- commands.c: update the cpl.container tags when opening/closing containers.
- item.c: Have locate_object see if the container matches the tag.  Don't have
  remove_item remove the object contents of other attributes if it is the
  container, but still remove it from the list it is on.  
- item.h: remove function prototypes - these are in proto.h
- png.c: New png -> X11 (or gdk) creation routines that are much faster.  This
  should make a noticable difference in performance.  Note that the X11
  and gdk implementations are very different now - the gdk implementation
  lets the gdk library do most of the work.
- gx11.c: remove some dead code, add call to gdk_rgb_init() if using
  png images - needed by new png loader.
- x11.c: Add call to init_pngx_loader if running in png mode.  Also pass 
  colormap by pointer so png_to_xpixmap can modify it.
- xutil.c: pass colormap by pointer to init_pngx_loader (same reason as above)
- Makefile.in: Add DMALLOC_LIB definition instead of it going in with the
  the default libraries.  cfsndserv will now get properly linked with
  dmalloc.
- configure.in, configure: add --disable-sound option, and make relevant
  changes to use that option (which basically amounts to not checking
  for any of the sound systems).  Add check for dmalloc.h.  change
  substitution for -ldmalloc.
- cfsndserv.c: Modified so it now compiles with the modern ALSA sound system.
  No idea if it actually works.  MSW 2001/03/04
- metaserver.c: Modified so it uses the value of -port if that command
  line option is given by a user.  MSW 2001/03/01
- x11.c: Fixes for info window resizing.  This should fix some crashes
  and the code is a bit simpler now.  MSW 2001/02/28
- Makefile.in: Modify so that installs the target (cfclient, gcfclient,
  cfsndserv) one at a time so it works with the install script.
- item.c: add insert_item_before_item function.  Modify the sorting function
  so it first sorts by type, then by locked/unlocked status, and then
 by alphabetical order (not including the number prefix).
- item_types, item_types.h: More updates of missing objects or ones that
  need more specific matching rules.
- x11.c: Remove a lot of duplicate code that was in place for metaserver
  support - instead, just add checks to the existing X event handling
  code to know not to do some things if we're in metaserver selection
  mode.  This fixes a bug in that resize events would not be handled
  if in metaserver selection mode.

* Tue Feb 13 2001 Bob Tanner <tanner@real-time.com>
- Rolled new client 0.96.0 with Mark's changes listed next

* Mon Feb 12 2001 Mark Wedel <mwedel@scruz.net>
- If compiled with dmalloc, add 'dmalloc command that verifies the heap.  Makes
  checking for memory corruption easier. 
- CHANGES, configure configure.in crossfire-client.spec: Update for 0.96.0
  release item_types item_types.h: Add some additional items.

* Sat Feb 10 2001 Bob Tanner <tanner@real-time.com>
- Created new Group for this package Amusements/Games/Crossfire

* Fri Feb 02 2001 Bob Tanner <tanner@real-time.com>
- Rolled new client with Mark's changes listed next 

* Tue Jan 30 2001 Mark Wedel <mwedel@scruze.net>
- Complete rewrite of the exit handling code.  Hopefully as an effect,
  this will fix the player appearing in the middle of the oceans.  I
  think the code should also work better in many other areas.  Main
  enhancements is a 3x3 area for pets to follow player to new map, as
  well as golems now following players to the new  maps. 
- include/sproto.h, random_maps/rproto.h - rebuilt.
- random_maps/random_map.c: Change generate_random_map to take a structure 
  with the random map paremeters.
- random_maps/reader.l, reader.c: Add set_random_map_variable function that
  reads the map parameters from a char buffer.  Also, remove some leftover
  comments that were from the common/loader.l file.
- random_maps/rogue_layout.c: Change some functions to be static so make proto
  doesn't collect them.
- random_maps/standalone.c: Add opening of parms file into main function since
  it ws removed from the random_map.c file.
- server/apply.c: Don't display the message of random maps to the players
  as they enter them, as this message is random map parameters, and not
  a real message.
- server/login.c: #if 0 out using of the player loading element in the
  structure.  this isn't used right now.
- server/main.c: Bulk of the changes.  main changes are to break apart
  the old enter_exit function into smaller functions that more
  logically do the needed function (random maps, unique maps, and
  transferring the player to the new map).  random map code now passes
  the parameters via structure instead of file in /tmp.  Code is much
  more understandable now and hopefully bugfree.
- server/pets.c: minor changes/bugfixes.  Search full SIZEOFFREE array, use
  real owner variable when print out messages.
- server/player.c: Remove usage of the loading variable in the player structure.

* Sun Jan 14 2001 Bob Tanner <tanner@real-time.com>
- Makefile.in: Create destination dirs, remove extra tab.  Patch also by Dave.
- Protocol: typo fixed.
- config.h, config.h.in: Add HVAE_DMALLOC_H #ifdefs.  Checks currently
  disable in configure.in, as with it, the sound won't like properly since
  it needs -ldmalloc, and I haven't bothered investing that much time
  into fixing the Makefile.
- gx11.c: Patches by Dave Peticolas - mostly code cleanup, but one new feature
  is support of wheel mice to move the scrollbars.
- png.c: No real code change, just adjustments in some ordering which I think
  makes the code appear a little simpler.
- x11.c: Minor code cleanups, some formatting changes, some to make better
  error messages.

* Wed Jan 08 2001 Bob Tanner <tanner@real-time.com>
- Applied MSW patch to change damge type to 16 bits

* Wed Jan 03 2001 Bob Tanner <tanner@real-time.com> [0.95.8-1]
- Upgraded client to 0.95.8
- Moved sounds into /usr/share/sounds/crossfire 
- Moved the prefix to /usr/X11R6
- Upgrade source file locations
- Made the gtk client GNOME aware and put the crossfire picture into
  Program->Games - Sounds are noarch

* Tue Mar 16 1999 Toshio Kuratomi <badger@prtr-13.ucsc.edu> [0.95.2-2]
- A few changes to conform to FHS 2.0
- Edit the sounds patch to place things in the /usr/share/sounds directory
  instead of /usr/share/sounds/sounds
- Rewrite certain parts of the script to rely more on the make install target
  rather than hacking it ourselves.
- We don't have to compile the program twice -- make all will create both the
  gtk and the Athena binaries.

* Sat Jan 30 1999 Kjetil Wiekhorst Jørgensen <jorgens+rpm@pvv.org> [0.95.2-1]
- upgraded to version 0.95.2
- fixed minor bug in Makefile

* Sat Jan 23 1999 Kjetil Wiekhorst Jørgensen <jorgens+rpm@pvv.org> [0.95.1-2]
- some bug fixes to the 0.95.1 release

* Tue Dec  8 1998 Kjetil Wiekhorst Jørgensen <jorgens+rpm@pvv.org> [0.95.1-1]
- upgraded to 0.95.1
- install sounds in /usr/share/sounds
- build both vanilla X client and GTK+ client

* Wed Dec  2 1998 Kjetil Wiekhorst Jørgensen <jorgens+rpm@pvv.org> [0.94.4-1]
- upgraded to 0.94.4

* Fri Sep  4 1998 Kjetil Wiekhorst Jørgensen <jorgens+rpm@pvv.org> [0.94.3-1]
- upgraded to version 0.94.3

* Tue Jun 02 1998 Kjetil Wiekhorst Jørgensen <jorgens+rpm@pvv.org>
- Initial release.
