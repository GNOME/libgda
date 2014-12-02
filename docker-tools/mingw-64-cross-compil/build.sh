#!/bin/sh

image_name="libgda-mingw64"

# test docker install
docker version > /dev/null 2>&1 || {
    echo "Can't find or execute docker"
    exit 1
}

# download Win64 binaries if necessary
if [ ! -d Win64 ]
then
    echo "Missing Win64/ directory, downloading archive (about 1Mb)..."
    tarball="https://people.gnome.org/~vivien/Win64_docker_02.txz"
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
fi

# build image
echo "Now building Docker image, this will take a few minutes (or maybe half an hour, depending on you setup)..."
docker build --force-rm -q -t "$image_name" . || {
    echo "Failed to build image."
    exit 1
}
echo "Image '$image_name' is now ready, you can use the start.sh script"

