#!/bin/sh

aclocal -I macros
autoheader
automake -a -c
autoconf
./configure "$@"
