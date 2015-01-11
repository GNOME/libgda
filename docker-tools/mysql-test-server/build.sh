#!/bin/sh

#docker_cmd="sudo docker"
docker_cmd="docker"
image_name="libgda-test-mysql"

# test docker install
$docker_cmd version > /dev/null 2>&1 || {
    echo "Can't find or execute docker"
    exit 1
}

# download Northwind data if necessary
sqlfile=setup-data/northwind.sql
if [ ! -e $sqlfile ]
then
    echo "Missing $sqlfile, downloading (about 0.8 Mb)..."
    file="https://people.gnome.org/~vivien/northwind_mysql_01.sql"
    wget -q $file > /dev/null 2>&1 || {
        echo "Unable to get $file, check with Libgda's maintainer!"
        exit 1
    }
    mv northwind_mysql_01.sql $sqlfile
    echo "Download complete"
fi

echo "Now building Docker image, this will take a few minutes (or maybe half an hour, depending on you setup)..."
$docker_cmd build --force-rm -q -t "$image_name" . || {
    echo "Failed to build image."
    exit 1
}
echo "Image '$image_name' is now ready, you can use the start.sh script"

