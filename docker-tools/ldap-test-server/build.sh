#!/bin/sh

image_name="libgda-test-ldap"

# test docker install
docker version > /dev/null 2>&1 || {
    echo "Can't find or execute docker"
    exit 1
}

# build image
echo "Generating LDAP directory contents"
./gen_names_ldif.pl || {
    echo "Error"
    exit 1
}

echo "Now building Docker image, this will take a few minutes (or maybe half an hour, depending on you setup)..."
docker build --force-rm -q -t "$image_name" . || {
    echo "Failed to build image."
    exit 1
}
echo "Image '$image_name' is now ready, you can use the start.sh script"

