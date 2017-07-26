#!/bin/sh
# win32-package -- run this in the build directory

function gather_deps() {
    mkdir -p lib share
    cp -R /mingw64/lib/gdk-pixbuf-2.0 lib
    cp -R /mingw64/lib/gtk-2.0 lib
    cp -R /mingw64/share/themes share
}

function gather_dlls() {
    dlls=`ldd $1 | grep mingw | awk '{print $3}'`
    for f in $dlls; do
        cp $f .
    done
}

set -e
make install
gather_deps
gather_dlls gtk-v2/src/crossfire-client-gtk2.exe
version="r`cd .. && svnversion`"
target="crossfire-client-$version-win`uname -m`"
mkdir -p $target
cp -R *.dll lib share $target
for bin in `ls bin`; do
    cp "bin/$bin" $target/$bin.exe
done
