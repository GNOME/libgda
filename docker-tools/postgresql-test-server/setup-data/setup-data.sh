#!/bin/bash

__mod_user() {
usermod -G wheel postgres
}

__create_db() {
su --login postgres --command "/usr/bin/postgres -D /var/lib/pgsql/data -p 5432" &
sleep 10

su --login - postgres --command "psql -c \"CREATE USER gdauser with CREATEROLE superuser PASSWORD 'gdauser';\""
su --login - postgres --command "psql -c \"CREATE DATABASE gda OWNER = gdauser;\""
su --login - postgres --command "psql -d gda -c \"\i /setup-data/northwind.sql;\""

killall postgres
}

# Call functions
__mod_user
__create_db
