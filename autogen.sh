#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="GNU Data Access"

(test -f $srcdir/configure.in \
  && test -d $srcdir/idl \
  && test -f $srcdir/idl/GNOME_Database.idl) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level GDA directory"
    exit 1
}

. $srcdir/macros/autogen.sh
