================
Crossfire Client
================

Crossfire is a free, open-source, cooperative multi-player RPG and adventure
game. Since its initial release, Crossfire has grown to encompass over 150
monsters, 3000 areas to explore, an elaborate magic system, 13 races, 15
character classes, and many powerful artifacts scattered far and wide. Set
in a fantastical medieval world, it blends the style of Gauntlet, NetHack,
Moria, and Angband.

- Website: http://crossfire.real-time.com/
- Wiki: http://wiki.cross-fire.org/


Installation
============
To build with the default options, change to the source directory and run::

    $ mkdir build && cd build/
    $ cmake ..
    $ make
    # make install

To build with minimal dependencies, use this CMake command instead::

    $ cmake -DLUA=OFF -DMETASERVER2=OFF -DSOUND=OFF ..

To build with debugging symbols::

    $ cmake -DCMAKE_BUILD_TYPE=Debug ..

Use **ccmake** instead of **cmake** to change these options and more
interactively.

For more details, see `Compiling the Crossfire Client <http://wiki.cross-fire.org/dokuwiki/doku.php/client:client_compiling>`_ on the Crossfire Wiki.

Dependencies
------------
- C compiler supporting C99
- CMake
- GTK+ 2
- libpng
- Perl
- pkg-config
- Vala

Optional:

- libcurl (for metaserver support)
- Lua 5 (for client-side Lua scripting)
- SDL2_mixer (for sound support)


Sounds
------
To play with sounds, make sure the client is compiled with ``SOUND``
enabled. Download the sound archive and extract it to
*${PREFIX}/share/crossfire-client*. Then enable sound effects in the client
preferences.


License
=======
This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

See *COPYING*.


Release
=======
To tag and prepare a client release:

#. Update *ChangeLog*
#. Update ``VERSION`` string in *CMakeLists.txt* and *gtk-v2/ui/dialogs.ui*
#. Create an annotated tag (e.g. ``git tag -a v1.2.3 -m "Tag 1.2.3 release"``)
#. Push tags
#. Build the client normally (see above)
#. Run ``git clean -fx`` to clean up local files
#. Download/check out current sounds into **sounds/** for inclusion in the source release
#. In the build directory, run ``make package_source``
#. Inspect resulting source tarball, extract, build, test
#. Upload source distribution, coordinate builds, announce
#. Celebrate with a Chateau Navar '78
