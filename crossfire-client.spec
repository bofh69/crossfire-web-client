%define Name crossfire
%define extra client
%define version 0.95.8
%define sndversion 0.95.4
%define release 1
%define prefix /usr/X11R6

Name: %{Name}-%{extra}
Version: %{version}
Release: %{release}
Summary: Client for connecting to crossfire servers.
Group: X11/Games
Copyright: GPL
Vendor: Crossfire Development Team
URL: http://crossfire.real-time.com
Packager: Crossfire Development Team <crossfire-devel@lists.real-time.com>
Source0 ftp://ftp.scruz.net/users/mwedel/public/crossfire-%{extra}-%{version}.tar.gz
Source1: ftp://ftp.scruz.net/users/mwedel/public/client-%{sndversion}-au-sounds.tgz
Source2: ftp://ftp.scruz.net/users/mwedel/public/client-%{sndversion}-raw-sounds.tgz
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

%prep
%setup -a 1 -a 2 -n crossfire-%{extra}-%{version}

%build
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

install sounds/* $RPM_BUILD_ROOT/usr/share/sounds/crossfire

install -c client.man $RPM_BUILD_ROOT/usr/X11R6/man/man1/cfclient.1
install -c client.man $RPM_BUILD_ROOT/usr/X11R6/man/man1/gcfclient.1
install -c client.gnome $RPM_BUILD_ROOT/usr/share/gnome/apps/Games/crossfire.desktop
install -c shield.png $RPM_BUILD_ROOT/usr/share/pixmaps/

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
%doc README CHANGES COPYING Protocol
%attr(755,root,root) /usr/X11R6/bin/cfclient
/usr/X11R6/man/man1/cfclient.1.gz

%files gtk
%defattr(644,root,root,755)
%doc README CHANGES COPYING Protocol
%attr(755,root,root) /usr/X11R6/bin/gcfclient
/usr/X11R6/man/man1/gcfclient.1.gz
/usr/share/gnome/apps/Games/crossfire.desktop
/usr/share/pixmaps/shield.png

%files sounds
%defattr(644,root,root,755)
/usr/share/sounds/crossfire/*

%changelog
* Wed Jan 03 2001 Bob Tanner<tanner@real-time.com> [0.95.8-1]
- Upgraded client to 0.95.8
- Moved sounds into /usr/share/sounds/crossfire 
- Moved the prefix to /usr/X11R6
- Upgrade source file locations
- Made the gtk client GNOME aware and put the crossfire picture into Program->Games
- Sounds are noarch

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
