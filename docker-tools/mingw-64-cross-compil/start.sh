#!/bin/sh

image_name="libgda-mingw64"

# test docker install
docker version > /dev/null 2>&1 || {
    echo "Can't find or execute docker"
    exit 1
}

# test docker image 
img=`docker images -q "$image_name"`
if test "x$img" == "x"
then
    echo "The docker image '$image_name' is not present, use the ./build.sh script first"
    exit 1
fi

# determine destination DIR
if test "$#" -eq 1
then
    destdir=$1
else
    destdir=/home/vivien/Devel/VMShared/bin-win64
fi
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
popd > /dev/null 2>& 1 || {
    echo "Can't get back to working directory!"
    exit 1
}
echo "Destination files will be in $destdir/libgda"

# get Libgda's sources dir
pushd ../.. > /dev/null 2>& 1 || {
    echo "Can't go to directory ../.."
    exit 1
}
gda_src=`pwd`
popd > /dev/null 2>& 1 || {
    echo "Can't get back to working directory!"
    exit 1
}

# get user and group ID
if test "x$SUDO_UID" == "x"
then
    uid=`id -u`
    gid=`id -g`
else
    uid=$SUDO_UID
    gid=$SUDO_GID
fi

echo "Using libgda's source files in:  ${gda_src}"
echo "Win64 compiled files will be in: ${destdir}"
echo "Once in the container, run:"
echo "# ./configure"
echo "# make"
echo "# make install"
echo "# ./do_packages"
echo ""
docker run -t -i -v ${gda_src}:/src/libgda:ro -v ${destdir}:/install -e UID=$uid -e GID=$gid --rm "$image_name"
