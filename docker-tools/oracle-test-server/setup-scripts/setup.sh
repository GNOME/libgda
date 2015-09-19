#!/bin/bash

. /setup-data/env.sh

# start server to create data
LISTENERS_ORA=/u01/app/oracle/product/$version/xe/network/admin/listener.ora

cp "${LISTENERS_ORA}.tmpl" "$LISTENERS_ORA" && 
sed -i "s/%hostname%/$HOSTNAME/g" "${LISTENERS_ORA}" && 
sed -i "s/%port%/1521/g" "${LISTENERS_ORA}" && 

service oracle-xe start

cd /setup-data
while :
do
    sqlplus -L system/gdauser@127.0.0.1/XE @setup.sql
    if test $? == 0
    then
	sqlplus gdauser/gdauser@127.0.0.1/XE @northwind.sql
	break;
    fi
    sleep 5
done

service oracle-xe stop
