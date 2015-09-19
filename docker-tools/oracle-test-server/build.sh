#!/bin/bash

# make sure Oracle XE's package is there
debfile=setup-data/oracle-xe_11.2.0-1.0_amd64.deb
if [ ! -e $debfile ]
then
	echo "Missing Oracle XE package $debfile, please download it from Oracle."
	exit 1
fi

# download Northwind data if necessary
sqlfile=setup-data/northwind.sql
if [ ! -e $sqlfile ]
then
    echo "Missing $sqlfile, downloading (about 0.7 Mb)..."
    file="https://people.gnome.org/~vivien/northwind_oracle_01.sql"
    wget -q $file > /dev/null 2>&1 || {
        echo "Unable to get $file, check with Libgda's maintainer!"
        exit 1
    }
    mv northwind_oracle_01.sql $sqlfile
    echo "Download complete"
fi

# actual build
exec ../docker-tools.sh build Oracle
