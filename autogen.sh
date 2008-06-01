#!/bin/sh

echo "Warning:"
echo "This script is deprecated, run autoreconf instead, then ./configure as usual."
echo
aclocal -I macros --install
autoheader
automake -a -c
autoconf
./configure "$@"
