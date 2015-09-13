#!/bin/sh

# the SRC, PREFIX and DEPEND variables are defined when building the Docker image

version="64"
src=$SRC/libgda
dest=$PREFIX/libgda
depend=$DEPEND

export CONFIGURE=$src/configure

if test "x$version" = "x64"
then
    echo "Windows 64 Build"
    ./.mingw-configure --enable-debug --prefix=$dest --with-mysql=$depend/mysql --with-bdb=$depend/bdb --with-mdb=yes --enable-system-mdbtools=no --with-oracle=$depend/oracle --with-firebird=no --with-postgres=/usr/x86_64-w64-mingw32/sys-root/mingw --with-java=no  --with-ldap=$depend/ldap --enable-vala=no --enable-vala-extensions=no
else
    echo "Not yet implemented!"
fi

rm -rf $dest/*
cp -R -L /src/libgda/installers/Windows /compilation/libgda/Packager 
