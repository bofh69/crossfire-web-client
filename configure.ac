# Process this file with autoconf to produce a configure script.

AC_INIT([crossfire-client],[2.0-dev])
AC_CONFIG_SRCDIR(crossfire-client.spec)

AC_CONFIG_AUX_DIR(utils)

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE

AC_CONFIG_HEADERS([common/config.h])

# AM_ACLOCAL_INCLUDE(macros)

# these are subdirectories that have make files in them
CF_SUBDIRS="common pixmaps utils help"

# Check for some neeed programs
AC_PATH_PROG(MKDIR, mkdir)
AC_PATH_PROG(TAR, tar)
AC_PATH_PROG(DEPEND, makedepend)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(RM, rm)
AC_PATH_PROG(PERL, perl)
AC_PATH_PROG(AR, ar)
AC_PATH_PROG(PKG_CONFIG, pkg-config)

# Check if svnversion is avaliable to get the rev number
AC_PATH_PROG(SVNVERSION, svnversion)

# Put this early on, since gtk may rely on it.
AC_PATH_XTRA

SOUNDDIR="${datadir}/crossfire-client/sounds"

# Add some options to 'configure'.
AC_ARG_ENABLE(debug, [AS_HELP_STRING([--enable-debug], [Turn on debugging (default=no)])],
if eval "test x$enable_debug = xyes"; then
    DEBUGFLAG="-g"
fi)

AC_ARG_ENABLE(ansi, [AS_HELP_STRING([--enable-ansi], [Turn on strict ansi (default=no)])],
                  , enable_ansi=$enableval)

AC_ARG_ENABLE(sound, [AS_HELP_STRING([--disable-sound], [Disable sound support (default=implement sound support if supported libraries exist)])],
                sound=$enableval, sound=yes )

# These are because some of us use OSS sound, even with ALSA or ALSA9 libs installed

AC_ARG_ENABLE(alsa9, [AS_HELP_STRING([--disable-alsa9], [Disable ALSA9 sound support (default=use alsa9 sound support if alsa9 libraries exist)])],
                alsa9=$enableval, alsa9=yes )

AC_ARG_ENABLE(alsa, [AS_HELP_STRING([--disable-alsa], [Disable ALSA sound support (default=use alsa sound support if alsa libraries exist)])],
                alsa=$enableval, alsa=yes )

AC_ARG_WITH(sound-dir, [AS_HELP_STRING([--with-sound-dir=dir], [Directory where the sounds are located (default=/usr/local/lib/sounds)])],
                SOUNDDIR="$withval")

# The follow two are really just a way to specify where the xpm information
# resides if not with the normal X11 stuff (ie, in /usr/local and not /usr
# /usr/X11

AC_ARG_WITH(ldflags, [AS_HELP_STRING([--with-ldflags=dir], [Provide additional linker directives to find libraries])],
                EXTRA_LIB="$withval")

AC_ARG_WITH(includes, [AS_HELP_STRING([--with-includes=dir], [Provide different compiler options to find headers with])],
                EXTRA_INC="$withval")

AC_ARG_WITH(loglevel, [AS_HELP_STRING([--with-logevel=level], [Sets default loglevel])],
                MINLOGLEVEL="$withval", MINLOGLEVEL=0)

AC_ARG_ENABLE(gtkv1, [AS_HELP_STRING([--disable-gtkv1], [Do not make original gtk client (default=make it if available)])],
                gtkv1=$enableval, gtkv1=yes )

AC_ARG_ENABLE(gtkv2, [AS_HELP_STRING([--disable-gtkv2], [Do not make gtk-v2 client (default=make it if available)])],
                gtkv2=$enableval, gtkv2=yes )

AC_ARG_ENABLE(cfgtk2, [AS_HELP_STRING([--enable-cfgtk2], [Enable building the original gtkv1 client with GTK+2])],
                cfgtk2=$enableval, cfgtk2=no)

AC_ARG_ENABLE(sdl, [AS_HELP_STRING([--disable-sdl], [Disable linking with the SDL library, default is to use it if available])],
                use_sdl=$enableval, use_sdl=yes)

AC_ARG_ENABLE(dmalloc, [AS_HELP_STRING([--enable-dmalloc], [Enable checking for dmalloc library, default is to use it if available])],
                use_dmalloc=$enableval, use_dmalloc=no)

AC_ARG_ENABLE(metaserver2, [AS_HELP_STRING([--disable-metaserver2], [Do not build in metaserver2 support])],
                metaserver2=$enableval, metaserver2=yes )

AC_PROG_CC

AC_C_BIGENDIAN

networklibs="yes"

case "$target" in
    alpha-dec-osf*)
        # If we are not using gcc, we want the ansi version of cc. 
        if test -z "$GCC"; then
        #   CFLAGS="$CFLAGS -std1 -warnprotos" # Not yet ready for warnprotos...
            CFLAGS="$CFLAGS -std1"
        fi
    ;;
    # SGI systems don't need -lnsl -lsocket
    mips*-sgi-irix*)
        networklibs="no"
    ;;
    # linux systems don't appear to need -lnsl
    *-linux-*)
        networklibs="no"
    ;;
    *-*-solaris2*)
        need_r_flag="yes"
    ;;
    *-*-mingw32*)
        win32="yes"
        # -mms-bitfields is needed for GTK+ in Windows.  It is set
        # automatically by pkg-config, but it is also needed in common, so we
        # explicitely set it
        CFLAGS="$CFLAGS -mms-bitfields"
        WIN32_LIBS="-lwsock32 -lwinmm"
        AC_SUBST(WIN32_LIBS)
    ;;
    *)
    ;;
esac

# Check for what clients we are going to build in order of x, gtk-v1, gtk-v2

# Check to see if X client will be built - no_x is set by AC_CHECK_XTRA if
# X11 isn't found on the machine
if eval "test x$no_x = xyes" ; then
    AC_MSG_WARN(X11 not found - will not build X client)
    X_LIBS=""
else
    AC_MSG_NOTICE(X11 found - will build X client)
    X_LIBS="$X_LIBS -lX11"
    CF_SUBDIRS="$CF_SUBDIRS x11"
    echo Xlibs: $X_LIBS
    echo Extras: $X_EXTRA_LIBS
    echo Xpre: $X_PRE_LIBS
fi    
# End of X client check

# Check to see if gtk-v1 client will be built
if eval "test x$gtkv1 = xyes" ; then
    if eval "test x$cfgtk2 = xyes" ; then
        PKG_CHECK_MODULES(GTK2, gtk+-2.0 >= 2.0.0, [HAVE_GTK2=yes], [HAVE_GTK2=no]) 
        if eval "test x$HAVE_GTK2 = xyes" ; then
            GTK_CFLAGS="`$PKG_CONFIG --cflags gtk+-2.0` -DCFGTK2 -DGTK_ENABLE_BROKEN -DGTK_WINDOW_DIALOG=GTK_WINDOW_TOPLEVEL"
            GTK_LIBS=`$PKG_CONFIG --libs gtk+-2.0`
            CF_SUBDIRS="$CF_SUBDIRS gtk"
            CFGTK="no"
            AC_MSG_NOTICE(GTK+ 2 found - building original gtk-v1 client with GTK+ 2.0.0)
        else
            AC_MSG_WARN(GTK+ 2 not found and --enable-cfgtk2 is set - cannot
build original gtk-v1 client)
            gtkv1="no"
        fi
    else
        AM_PATH_GTK(1.0.0,CFGTK="yes",CFGTK="no")
        if eval "test x$CFGTK = xyes"; then
             AC_MSG_NOTICE(GTK 1 found - building gtk client)
             GTK_LIBS=$GTK_LIBS
             GTK_CFLAGS="$GTK_CFLAGS"
             AC_SUBST(GTK_LIBS)
             AC_SUBST(GTK_CFLAGS)
             CF_SUBDIRS="$CF_SUBDIRS gtk"
             # AC_DEFINE(GTK_CLIENT, 1, [Define if the GTK client is built.])
        else
            AC_MSG_WARN(GTK 1 not found - not building building gtk-v1 client)
            gtkv1=no
        fi
    fi
else
    AC_MSG_NOTICE(gtkv1 client build disabled)
fi
AC_SUBST(CFGTK)
# End of gtk-v1 client check

# Check to see if gtk-v2 client will be built
if eval "test x$gtkv2 = xyes" ; then
    if eval "test x$HAVE_GTK2 != xyes" ; then
        # Check for GTK+ 2
        PKG_CHECK_MODULES(GTK2, gtk+-2.0 >= 2.0.0, [HAVE_GTK2=yes], [HAVE_GTK2=no]) 
    fi
    if eval "test x$HAVE_GTK2 = xyes" ; then
        # Check for Libglade-2.0
        PKG_CHECK_MODULES(LIBGLADE, libglade-2.0 >= 2.0.0, [HAVE_LIBGLADE=yes], [HAVE_LIBGLADE=no]) 
        if eval "test x$HAVE_LIBGLADE = xyes" ; then
            CF_SUBDIRS="$CF_SUBDIRS gtk-v2"
            AC_SUBST(LIBGLADE_CFLAGS)
            AC_SUBST(LIBGLADE_LIBS)
            AC_MSG_NOTICE(GTK+-2.0 & Libglade-2.0 found; will build gtk-v2 client)
        else
            AC_MSG_WARN(Libglade-2.0 libraries not found; will not build gtk-v2 client)
            gtkv2="no"
        fi
    else
        AC_MSG_WARN(GTK+ 2 libraries not found; will not build gtk-v2 client)
        gtkv2="no"
    fi
else
    AC_MSG_NOTICE(gtkv2 client disabled)
fi
# End of gtk-v2 client check

# End of client checks

AM_MAINTAINER_MODE
AC_SUBST(PACKAGE_LIBS)
AC_SUBST(INTLLIBS)

# sound.c needs sqrt.  So does png (on some systems)
# lua also needs it, so put it near the top
AC_CHECK_LIB(m, sqrt)

AC_CHECK_LIB(pthread, pthread_create)

# Check for lua 5.0 or 5.1.  See also: http://lua-users.org/wiki/AutoConfig
AC_CHECK_LIB(lua, lua_call,
[   # lua found
    AC_CHECK_LIB(lualib, luaopen_loadlib,
    [   # 5.0 needed lualib.
        lua_scripting="yes"
	LUA_LIBS="-llua -llualib -ldl -lm"
    ],
    [   # We do not have lualib, so it might be 5.1
	AC_CHECK_LIB(lua, luaL_register,
        [   # Lua 5.1 found
            lua_scripting="yes"
	    AC_DEFINE(HAVE_LUA_5_1, 1, [Define to use Lua 5.1])
        ],
        [   # No, it is not 5.1.  Now what?
	    AC_MSG_WARN(Linking with liblualib failed.)
        ])
    ],
    [
        -llua -ldl -lm
    ])
])
if eval "test x$lua_scripting != xyes" ; then
    AC_DEFINE(HAVE_LUA, 1, [Define to use Lua 5.x])
    lua_scripting="no"
    LUA_LIBS=""
fi
AC_SUBST(LUA_LIBS)

# Check for OpenGL
AC_CHECK_LIB(GL, main)
AC_CHECK_LIB(GLU, main)
AC_CHECK_LIB(opengl32, main)
AC_CHECK_LIB(glu32, main)
AC_CHECK_LIB(glut, main, [opengl="yes"; AC_DEFINE(HAVE_OPENGL, 1, [Define if you have OpenGL.])], [opengl="no"])
if eval "test x$opengl = xyes" ; then
    if eval "test x$win32 = xyes" ; then
        OPENGL_LIBS="-lopengl32 -lglu32 -lglut"
    else
        OPENGL_LIBS="-lGL -lGLU -lglut"
    fi
    AC_SUBST(OPENGL_LIBS)
fi
# End of OpenGL checks

# Checks for programs.
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_CPP
AC_PROG_RANLIB

AC_SUBST(X_LIBS)
AC_SUBST(X_CFLAGS)

CPPFLAGS="$CPPFLAGS $EXTRA_INC"
LDFLAGS="$LDFLAGS $EXTRA_LIB"
if eval "test x$need_r_flag = xyes"; then
    LDFLAGS="$LDFLAGS"
fi

if eval "test x$sound = xyes"; then
    if eval "test x$alsa = xyes"; then
        AC_CHECK_LIB(asound, main, alsa_sound=yes, , -lm)
    fi
    AC_CHECK_LIB(audio, alNewConfig, sgi_sound=yes, )
    if eval "test x$alsa9 = xyes"; then
        AC_CHECK_HEADER(alsa/asoundlib.h, alsa9_sound=yes, alsa_sound=no)
    fi
    AC_CHECK_HEADER(sys/soundcard.h, oss_sound=yes, oss_sound=no)
    AC_CHECK_HEADER(sys/audioio.h, sun_sound=yes, sun_sound=no)

    # alsa9 sound support is now independent of other sound systems,
    # so don't preclude compilation of others just because
    # we find it.

    if eval "test x$alsa9_sound = xyes"; then
        echo "Using ALSA sound system (0.9.x)"
        CFLAGS="$CFLAGS -DALSA9_SOUND"
        ALSA9_TARGET='cfsndserv_alsa9$(EXEEXT)'
        ALSA9_LIBS="-lasound -lm"
        SOUND_SRC="sound-src";

        # It appears that alsa9 sound while having the same headers, is
        # not backwards compatible, so if you have alsa 0.9 or later, you
        # can't compile the old old sound daemon, so if we find alsa9,
        # disable old alsa.
        alsa_sound="no";
    fi

    if eval "test x$alsa_sound = xyes"; then
        echo "Using ALSA sound system"
        CFLAGS="$CFLAGS -DALSA_SOUND"
        SND_LIBS="-lasound -lm"
        SOUND_SRC="sound-src";
    else 
        if eval "test x$sgi_sound = xyes"; then
            echo "Using SGI sound system"
            CFLAGS="$CFLAGS -DSGI_SOUND"
            SND_LIBS="-laudio"
            SOUND_SRC="sound-src";
        else
            if eval "test x$oss_sound = xyes"; then
                echo "Using OSS sound system"
                CFLAGS="$CFLAGS -DOSS_SOUND"
                SND_LIBS="-lm"
                AC_CHECK_LIB(ossaudio, main, SND_LIBS="$LIBS -lossaudio")
                SOUND_SRC="sound-src";
            else
                if eval "test x$sun_sound = xyes"; then
                    echo "Using solaris sound system"
                    CFLAGS="$CFLAGS -DSUN_SOUND"
                    SND_LIBS="-lm"
                    SOUND_SRC="sound-src";
                else
                    echo "No suitable sound system found."
                    sound="no"
                fi
            fi
        fi
    fi
    # this fi is for the sound == yes check above
fi

# basically, only want to put sound-src once in the
# subdirs listing.
if eval "test x$SOUND_SRC != x"; then
    CF_SUBDIRS="$CF_SUBDIRS $SOUND_SRC";
fi

AC_SUBST(GUI_OBJS)
AC_SUBST(GUI_SRCS)
AC_SUBST(SND_LIBS)
AC_SUBST(LDFLAGS)
AC_SUBST(TARGET)
AC_SUBST(CF_SUBDIRS)
AC_SUBST(ALSA9_TARGET)
AC_SUBST(ALSA9_LIBS)

# Check for SDL 1.1.3 and sdl_image
# note SDL_image does not ship with an sdl-image-config
# so I'll just assume it is in the same dir as SDL
if eval "test x$use_sdl = xyes"; then
    AM_PATH_SDL(1.1.3)

    if eval "test x$no_sdl = x"; then
        AC_CHECK_LIB( SDL_image, IMG_LoadPNG_RW,
            have_sdlimage="yes", have_sdlimage="no", $SDL_CFLAGS $SDL_LIBS) 
        if eval "test x$have_sdlimage = xyes"; then
            echo "sdl image detected!"
            SDL_LIBS="$SDL_LIBS -lSDL_image"
        else
            no_sdl="yes"
        fi
    fi

    if eval "test x$no_sdl = x"; then
        AC_DEFINE(HAVE_SDL, 1, [Define if you have SDL])
    else
        use_sdl="no"
    fi
fi

AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

# The following hacks for modifying CFLAGS were borrowed from the GIMP.
if test -n "$DEBUGFLAG"; then
    CFLAGS="$DEBUGFLAG $CFLAGS"
fi

if eval "test x$GCC = xyes"; then
    if echo "$CFLAGS" | grep "\-Wall" > /dev/null 2> /dev/null; then
        CFLAGS="$CFLAGS"
    else
        CFLAGS="$CFLAGS -Wall"
    fi

    if eval "test x$enable_ansi = xyes"; then
        if echo "$CFLAGS" | grep "\-ansi" > /dev/null 2> /dev/null; then
            CFLAGS="$CFLAGS"
        else
            CFLAGS="$CFLAGS -ansi"
        fi

        if echo "$CFLAGS" | grep "\-pedantic" > /dev/null 2> /dev/null; then
            CFLAGS="$CFLAGS"
        else
            CFLAGS="$CFLAGS -pedantic"
        fi
    fi
fi

# We need to check for socket/nsl here - some versions of the windowing
# systems need these to properly link with X (I thought AC_PATH_XTRA should
# take care of this, but apparently is does not).  If it is not here,
# the check for xpm fails, and configure quits.

if eval "test x$networklibs = xyes"; then
    AC_CHECK_LIB(nsl, main, LIBS="$LIBS -lnsl")
    AC_CHECK_LIB(socket, main, LIBS="$LIBS -lsocket")
fi

# Save and modify CPPFLAGS and LIBS (to include X and GTK paths temporarily).
gcfc_save_CPPFLAGS="$CPPFLAGS"
gcfc_save_LIBS="$LIBS"
CPPFLAGS="$GTK_CFLAGS $CPPFLAGS $X_CFLAGS $EXTRA_INC"
LIBS="$LIBS $GTK_LIBS $X_LIBS"

# Put this here so we get the gdk libs
# AC_CHECK_LIB(gdk_imlib, main,GTK_LIBS="$GTK_LIBS -lgdk_imlib")

# Need to check for Xext while we still have X_LIBS set.
AC_CHECK_LIB(Xext, main, X_LIBS="$X_LIBS -lXext")

# AC_CHECK_LIB(Imlib, main, X_LIBS="$X_LIBS -lImlib")

# Restore previous CPPFLAGS and LIBS.
CPPFLAGS="$gcfc_save_CPPFLAGS"
LIBS="$gcfc_save_LIBS"

LIBCURL_CHECK_CONFIG()
LIBS="$LIBS $LIBCURL"
CPPFLAGS="$CPPFLAGS $LIBCURL_CPPFLAGS"

AC_SUBST(LIBS)

# png on some systems need the zlib, so check for it
AC_CHECK_LIB(z, main)

AC_CHECK_LIB(png, main, , [AC_MSG_ERROR([You must have the png library installed to compile the client])])

# not strictly needed, but since the X11 client uses it, lets use it here

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h sys/ioctl.h sys/time.h unistd.h string.h sys/select.h pthread.h curl/curl.h)

# Metaserver support is pretty critical to the client, so fail unless the
# user explicity tells us to compile it without metaserver2 support.
if eval "test x$ac_cv_header_curl_curl_h = xno" ; then
    if eval "test x$metaserver2 = xyes"; then
        AC_MSG_ERROR([curl/curl.h header not found, but metaserver2 support is enabled.  Install header file or use --disable-metaserver2]);
    fi
fi

# this is used to control default behavior when client starts up - even
# if user has necessary files, if they use --disable-metaserver2, they
# won't try to get data from those servers
if eval "test x$metaserver2 = xyes"; then
    AC_DEFINE_UNQUOTED(METASERVER2, 1, [If metaserver2 support is enabled.])
else
    AC_DEFINE_UNQUOTED(METASERVER2, 0, [If metaserver2 support is enabled.])
fi

if eval "test x$use_dmalloc = xyes"; then
    AC_CHECK_LIB(dmalloc, main, DMALLOC_LIB="-ldmalloc")
    AC_SUBST(DMALLOC_LIB)
    AC_CHECK_HEADERS(dmalloc.h)
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_TIME

AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(mkdir socket strcspn sysconf getaddrinfo)

AC_SUBST(XPM)
AC_SUBST(SOUNDDIR)
AC_SUBST(LIBXPM_LIB)

# we need to run evals on the original variable names so that
# they get expanded to the actual path - otherwise, the substitution
# below will contain the unexpanded variables that don't resolve.

test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

ndatadir=`eval echo ${datadir}`
ndatadir=`eval echo ${ndatadir}`

# need to do this twice to cover variables within variables
nbindir=`eval echo ${bindir}`
nbindir=`eval echo ${nbindir}`

# Datadir should be working directory in Windows, as there is no set
# installation directory (choosable at installation time)
if eval "test x$win32 = xyes" ; then
    AC_DEFINE_UNQUOTED(CF_DATADIR, ".", [Location of read-only machine independent data.])
else
    AC_DEFINE_UNQUOTED(CF_DATADIR, "${ndatadir}/crossfire-client", [Location of read-only machine independent data.])
fi
AC_DEFINE_UNQUOTED(BINDIR, "${nbindir}", [Location of other binaries.])
AC_DEFINE_UNQUOTED(MINLOGLEVEL, ${MINLOGLEVEL}, [Default logging level.])

AC_SUBST(CF_DATADIR)

# Make all makefiles, even if CF_SUBDIRS are not set for that directory.
# since CF_SUBDIRS won't be set in the top level makefile, we don't
# descend into these subdirs.

AC_CONFIG_FILES([
    Makefile gtk/Makefile x11/Makefile common/Makefile 
    sound-src/Makefile gtk-v2/Makefile pixmaps/Makefile
    gtk-v2/glade/Makefile gtk-v2/src/Makefile gtk-v2/themes/Makefile
    help/Makefile utils/Makefile ])
AC_OUTPUT

AC_MSG_NOTICE([])
AC_MSG_NOTICE([Configuration summary....])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  Paths])
AC_MSG_NOTICE([    prefix default value                 ${prefix}])
AC_MSG_NOTICE([    exec_prefix default value            ${exec_prefix}])
AC_MSG_NOTICE([    Will put executables in              $bindir])
AC_MSG_NOTICE([    Will put config in                   $sysconfdir])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([  Build options])
if eval "test x$cfgtk2 = xyes" ; then
  gtk1_2_ext="(with gtk2 extensions)"
else
  gtk1_2_ext=""
fi
AC_MSG_NOTICE([    Will build GTK1 client?              $gtkv1 $gtk1_2_ext])
AC_MSG_NOTICE([    Will build GTK2 client?              $gtkv2])
AC_MSG_NOTICE([    Will build OpenGL renderer?          $opengl])
AC_MSG_NOTICE([    Will build SDL renderer?             $use_sdl])
sound_system=""
if eval "test x$sound = xyes"; then
  if eval "test x$alsa9_sound = xyes"; then
    sound_system="$sound_system (Alsa 0.9.x)"
  fi
  if eval "test x$alsa_sound = xyes"; then
    sound_system="$sound_system (Alsa)"
  fi
  if eval "test x$sgi_sound = xyes"; then
    sound_system="$sound_system (SGI)"
  fi
  if eval "test x$oss_sound = xyes"; then
    sound_system="$sound_system (OSS)"
  fi
  if eval "test x$sun_sound = xyes"; then
    sound_system="$sound_system (Solaris)"
  fi
fi
AC_MSG_NOTICE([    Will build sound server?             $sound $sound_system])

AC_MSG_NOTICE([])
AC_MSG_NOTICE([  Scripting options])
AC_MSG_NOTICE([    Will include lua interface?          $lua_scripting])
