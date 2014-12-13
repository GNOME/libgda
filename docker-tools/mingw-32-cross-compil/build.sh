#!/bin/sh

#docker_cmd="sudo docker"
docker_cmd="docker"
image_name="libgda-mingw32"

# test docker install
$docker_cmd version > /dev/null 2>&1 || {
    echo "Can't find or execute docker"
    exit 1
}

# download Win32 binaries if necessary
if [ ! -d Win32 ]
then
    echo "Missing Win32/ directory, downloading archive (about 3Mb)..."
    tarball="https://people.gnome.org/~vivien/Win32_docker_02.txz"
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
$docker_cmd build --force-rm -q -t "$image_name" . || {
    echo "Failed to build image."
    exit 1
}
echo "Image '$image_name' is now ready, you can use the start.sh script"

