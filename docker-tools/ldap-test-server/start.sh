#!/bin/sh

image_name="libgda-test-ldap"

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

echo "Running LDAP server, hit CTRL-C to stop"
docker run -t -i -p 389:389 --rm --name test-ldap "$image_name"
