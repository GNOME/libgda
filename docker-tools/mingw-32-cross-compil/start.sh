#!/bin/bash

# determine destination DIR
destdir="`pwd`/compilation-results"
if [ ! -e $destdir ]
then
    echo "Destination directory '$destdir' does not exist"
    exit 1
fi
pushd $destdir > /dev/null 2>& 1 || {
    echo "Can't go to directory $destdir"
    exit 1
}
destdir=`pwd`
rm -rf libgda 2>& 1 || {
    echo "Can't clean any previous build!"
    exit 1
}
popd > /dev/null 2>& 1 || {
    echo "Can't get back to working directory!"
    exit 1
}
echo "Destination files will be in $destdir/libgda"

# determine Libgda's sources dir
pushd ../.. > /dev/null 2>& 1 || {
    echo "Can't go to directory ../.."
    exit 1
}
gda_src=`pwd`
popd > /dev/null 2>& 1 || {
    echo "Can't get back to working directory!"
    exit 1
}
if [ -e $gda_src/Makefile ]
then
    echo "Source directory already configured; run \"make distclean\" there first"
    exit 1
fi

export gda_src
export destdir

echo "Using libgda's source files in:  ${gda_src}"
echo "Win32 compiled files will be in: ${destdir}"
echo "Once in the container, run:"
echo "# ./configure"
echo "# make"
echo "# make install"
echo "# ./do_packages"
echo ""
exec ../docker-tools.sh start MinGW32
