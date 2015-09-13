#!/bin/bash

# download Win64 binaries if necessary
if [ ! -d setup-data/Win64 ]
then
    echo "Missing Win64/ directory, downloading archive (about 1Mb)..."
    tarball="https://people.gnome.org/~vivien/Win64_docker_02.txz"
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
exec ../docker-tools.sh build MinGW64
