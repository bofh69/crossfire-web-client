%define Name crossfire
%define extra client
%define version 1.1.0
%define sndversion 1.1.0
%define release 1
%define prefix /usr/X11R6

Name: %{Name}-%{extra}
Version: %{version}
Release: %{release}
Summary: Client for connecting to crossfire servers.
Group: Amusements/Games/Crossfire
Copyright: GPL
Vendor: Crossfire Development Team
URL: http://crossfire.real-time.com
Packager: Crossfire Development Team <crossfire-devel@lists.real-time.com>
Source0: ftp://ftp.sourceforge.net/pub/sourceforge/crossfire/crossfire-client-%{version}.tar.gz
Source1: ftp://ftp.sourceforge.net/pub/sourceforge/crossfire/crossfire-sounds-%{sndversion}.tar.gz
BuildRoot: /var/tmp/%{Name}-%{extra}-%{version}-root

%description
Crossfire is a highly graphical role-playing adventure game with
characteristics reminiscent of rogue, nethack, omega, and gauntlet. 
It has multiplayer capability and presently runs under X11.

Client for playing the new client/server based version of Crossfire.
This package allows you to connect to crossfire servers around the world.
You do not need to install the crossfire program in order to use this
package.

%package sounds
Summary: Sound effects for the crossfire game
Group: X11/Games

%description sounds
Sound effects for people who want sounds with their game.

%package gtk
Summary: GTK client for %{Name}
Group: X11/Games

%description gtk
GTK version of the crossfire client

#Not supported yet
#%package gnome
#Summary:gnome client for %{Name}
#Group: X11/Games
#
#%description gnome
#gnome version of the crossfire client

%prep
%setup -a 1 -n %{Name}-client-%{version}

%build

chmod 755 configure
CFLAGS="$RPM_OPT_FLAGS" \
./configure --prefix=/usr/X11R6 --exec-prefix=/usr/X11R6/bin \
    --bindir=/usr/X11R6/bin --mandir=/usr/X11R6/man \
    --with-sound-dir=/usr/share/sounds/crossfire
make

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT/usr/X11R6/bin
install -d $RPM_BUILD_ROOT/usr/X11R6/man/man1
install -d $RPM_BUILD_ROOT/usr/share/sounds/crossfire
install -d $RPM_BUILD_ROOT/usr/share/gnome/apps/Games
install -d $RPM_BUILD_ROOT/usr/share/pixmaps


make install \
    DESTDIR=$RPM_BUILD_ROOT \
    bindir=$RPM_BUILD_ROOT/usr/X11R6/bin \
    mandir=$RPM_BUILD_ROOT/usr/X11R6/man/man1

install %{Name}-sounds-%{sndversion}/* $RPM_BUILD_ROOT/usr/share/sounds/crossfire

install -c x11/cfclient.man $RPM_BUILD_ROOT/usr/X11R6/man/man1/cfclient.1
install -c gtk/gcfclient.man $RPM_BUILD_ROOT/usr/X11R6/man/man1/gcfclient.1
# Not supported yet
# install -c gnome/gnome-cfclient.man $RPM_BUILD_ROOT/usr/X11R6/man/man1/gnome-cfclient.1

install -c gnome/client.gnome $RPM_BUILD_ROOT/usr/share/gnome/apps/Games/crossfire.desktop
install -c pixmaps/shield.png $RPM_BUILD_ROOT/usr/share/pixmaps/


%clean
rm -rf $RPM_BUILD_ROOT
# Cannot figure out how to get just the sounds to build as noarch, so this
# is a hack to make it work
mv %{_rpmdir}/%{_arch}/%{Name}-client-sounds-%{sndversion}-%{release}.%{_arch}.rpm %{_rpmdir}/noarch/%{Name}-client-sounds-%{sndversion}-%{release}.noarch.rpm

%files
%defattr(644,root,root,755)
%doc CHANGES COPYING License NOTES README TODO
%attr(755,root,root) /usr/X11R6/bin/cfclient
/usr/X11R6/man/man1/cfclient.1*

%files gtk
%defattr(644,root,root,755)
%doc CHANGES COPYING License NOTES README TODO
%attr(755,root,root) /usr/X11R6/bin/gcfclient
/usr/X11R6/man/man1/gcfclient.1*
/usr/share/gnome/apps/Games/crossfire.desktop
/usr/share/pixmaps/shield.png

# Not supported yet
#%files gnome
#%defattr(644,root,root,755)
#%doc CHANGES COPYING License NOTES README TODO
#%attr(755,root,root) /usr/X11R6/bin/gnome-cfclient
#/usr/X11R6/man/man1/gnome-cfclient.1*
#/usr/share/gnome/apps/Games/crossfire.desktop
#/usr/share/pixmaps/shield.png

%files sounds
%defattr(644,root,root,755)
/usr/share/sounds/crossfire/*

%changelog
* Mon Dec 31 2001 Bob Tanner <tanner@real-time.com>
- Rolled 1.1.0 client
- NOTE Mark's new email address
- Fixed typo in install target for x11 client.

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
- Makefile.in: Modify so that it installs the target (cfclient, gcfclient,
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
