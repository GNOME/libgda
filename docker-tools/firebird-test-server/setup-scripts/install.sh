#!/bin/bash

# avoid dpkg frontend dialog / frontend warnings 
export DEBIAN_FRONTEND=noninteractive

# install firebird
apt-get update && apt-get install -y firebird2.5-superclassic firebird2.5-examples

# unzip sample data and prepare alias for the "gda" database
pushd /usr/share/doc/firebird2.5-common-doc/examples/empbuild/ && gunzip employee.fdb.gz && popd
echo "gda = /var/lib/firebird/gda.fdb" >> /etc/firebird/2.5/aliases.conf

# have the server listen on any interface
sed -i -e 's/#\(RemoteBindAddress =\)/\1/' -e 's/\(RemoteBindAddress = l\)/#\1/' /etc/firebird/2.5/firebird.conf

isql-fb -u SYSDBA -p masterkey employee -i /setup-data/setup.sql > /LOG 2>&1
isql-fb -u gdauser -p gdauser gda -i /setup-data/northwind.sql > /LOG 2>&1
