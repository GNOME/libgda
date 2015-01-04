#!/bin/sh

cd /setup-data

password="gdauser"

/usr/bin/mysqladmin -u root password "$password"
cat user.sql | sed -e "s/PASSWORD/$password/" | /usr/bin/mysql -u root --password=$password

# loading data
/usr/bin/mysql -u gdauser --password=$password < northwind.sql

# shutdown server
/usr/bin/mysqladmin -u root --password=$password shutdown
