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

    $ cmake -DLUA=OFF -DMETASERVER2=OFF -DOPENGL=OFF -DSDL=OFF -DSOUND=OFF ..

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
- Vala

Optional:

- libcurl (for metaserver support)
- Lua 5 (for client-side Lua scripting)
- OpenGL (gl, glu, glx) (for OpenGL rendering)
- SDL, SDL_image (for SDL rendering)
- SDL_mixer (for sound support)


Sounds
------
To play with sounds, make sure the client is compiled with ``SOUND``
enabled. Download the sound archive and extract it to
*${PREFIX}/share/crossfire-client*. Then enable sound effects in the client
preferences.


Preloaded Bitmaps
-----------------
.. note:: This legacy documentation does not necessarily reflect the behavior of the current version of the client.

The client will get any images that it is missing from the server.  This
can include the entire image set.

To decrease bandwidth used when actually playing the sounds, it is suggested
you download the image archive and install it.  The default location
for the archive is *<prefix>/share/cfclient/*, where *<prefix>* is determined
by the -prefix= option given when running configure.

The mechanism the client uses to find a matching image file when the
server tells it an image name is thus:

- Look in *~/.crossfire/gfx*.  If an image is found here, irrespective of
  the set and checksum of the image on the server, it will be used.  The only
  way images are placed into the gfx directory is by the user actually
  copying them to that directory.  The gfx directory allows a user to
  override images with versions he prefers.
- Look in *~/.crossfire/image-cache* then
  '<prefix>/share/cfclient/crossfire-images'. If the checksum matches the
  image from the respective directory is used. Note that if the checksums
  match, it really doesn't matter what image location we use, as it is the
  same image.
- Get the image from the server. If -cache is set, a copy of it is put into
  *~/.crossfire/image-cache*.


License
=======
This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

See *COPYING*.


Authors
=======
.. include:: AUTHORS
