#!/bin/bash

# download Win32 binaries if necessary
if [ ! -d setup-data/Win32 ]
then
    echo "Missing Win32/ directory, downloading archive (about 3Mb)..."
    tarball="https://people.gnome.org/~vivien/Win32_docker_02.txz"
    pushd setup-data > /dev/null 2>&1
    wget -q $tarball > /dev/null 2>&1 || {
	echo "Unable to get $tarball, check with Libgda's maintainer!"
	exit 1
    }
    echo "Download complete"
    file=`basename $tarball`
    tar xJf $file > /dev/null 2>&1 || {
	echo "Unable to uncompress $file."
	exit 1
    }
    rm -f $file
    popd > /dev/null 2>&1
fi

# actual build
exec ../docker-tools.sh build MinGW32
