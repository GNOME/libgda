#!/bin/sh

#
# This script tests Blob implementation. It was first designed for PostgreSQL but does not
# depend on it.
#
# It requires:
# - a DSN named ${prefix}blobs where ${prefix} is set to the database provider being tested later
# - the DSN must point to a database which at least has the 'blobs' table created as:
#   Postgresql:    CREATE TABLE blobs (id int, descr VARCHAR, blob OID);
#   H2:            CREATE TABLE blobs (id int, descr VARCHAR, blob BLOB);
#   SQLite:        CREATE TABLE blobs (id int, descr string, blob blob);
#   note: the OID type is the data type for blobs for PostgreSQL
# - it must be run in the tools/ directory of Libgda's sources since it loads the gda-sql.c and gda-sql.ico
#   files.
#
# Note: the 'blobs' table's contents will be overriden by the test

prefix=pg
#prefix=h2
#prefix=sqlite

rm -f EXPORT_gda_sql_c EXPORT_gda_sql_ico
./gda-sql-4.1 ${prefix}blobs <<EOF
delete from blobs;
.setex bl gda-sql.c
insert into blobs (id, descr, blob) values (1, 'descr1', ##bl::GdaBlob);
insert into blobs (id, descr, blob) values (10, 'descr1', ##bl::GdaBlob);
#select * from blobs;
.setex bl2 gda-sql.ico
insert into blobs (id, descr, blob) values (2, 'descr2', ##bl2::GdaBlob);
.unset
.setex bl blobs blob id=2
update blobs set descr='copied from id=2', blob= ##bl::GdaBlob WHERE id=1;
insert into blobs (id, descr, blob) values (3, 'inserted from id=2', ##bl::GdaBlob);
#select * from blobs;
.unset
.setex bl10 blobs blob id=10
.export bl10 EXPORT_gda_sql_c
.export blobs blob id=2 EXPORT_gda_sql_ico
.export blobs blob id=1 EXPORT_gda_sql_ico1
EOF
if test $? != 0
then
	echo "ERROR executing script"
fi
cmp EXPORT_gda_sql_c gda-sql.c
if test $? != 0
then
        echo "BLOB export failed for gda-sql.c"
	exit 1
fi
cmp EXPORT_gda_sql_ico gda-sql.ico
if test $? != 0
then
        echo "BLOB export failed for gda-sql.ico"
	exit 1
fi
cmp EXPORT_gda_sql_ico1 gda-sql.ico
if test $? != 0
then
        echo "BLOB export failed for gda-sql.ico (ID=1)"
	exit 1
fi

rm -f EXPORT_gda_sql_c EXPORT_gda_sql_ico EXPORT_gda_sql_ico1
echo "Test successfull"
