#!/bin/bash

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

# actual build
exec ../docker-tools.sh build MySQL
