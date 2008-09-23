#!/bin/sh

#
#
# This script creates a .ZIP file containing a minimum set of files required
# to execute the GDA SQL console.
#
# It is made to be used when cross compiling
#
# Variables to set are:
# $cross_path is the location of the cross-compilation environment
# $depend_path is the location of the dependencies (GLib, DBMS client libraries, etc)
# $prefix is the location of the compiled and installed Libgda's sources
# $version is the current Libgda's version
#

cross_path=/fillme
depend_path=/local/Win32
prefix=/fillme
version=3.99.5



#
# no modification below
#
current_dir=`pwd`
archive=${current_dir}/gda-sql-${version}.zip


# remove current archive if it exists
rm -f $archive

#
# Takes 3 arguments:
# $1 = the prefix directory where files are located
# $2 = the prefix under $1 where all the files are
# $3 = a list of files physically in $1/$2/
#
function add_files_to_zip
{
    dir=$1
    subdir=$2
    files=$3
    pushd $dir >& /dev/null
    for item in ${files[*]}
    do
	echo "Adding file $dir/$subdir/$item"
	zip ${archive} $subdir/$item >& /dev/null
	if [ $? -ne 0 ]
	then
	    echo "Error (file may noy exist)"
	    exit 1
	fi
    done
    popd >& /dev/null
}

#
# dependencies DLLs
#
files=(charset.dll iconv.dll intl.dll libgio-2.0-0.dll libglib-2.0-0.dll libgmodule-2.0-0.dll libgobject-2.0-0.dll libgthread-2.0-0.dll libxml2.dll zlib1.dll)
add_files_to_zip ${depend_path}/gtk bin $files

files=(libdb47.dll msvcp80.dll msvcr80.dll)
add_files_to_zip ${depend_path}/bdb bin $files

files=(libmdb-0.dll)
add_files_to_zip ${depend_path}/mdb bin $files

files=(libmySQL.dll)
add_files_to_zip ${depend_path}/mysql bin $files

files=(libpq.dll comerr32.dll gssapi32.dll k5sprt32.dll krb5_32.dll libeay32.dll libiconv2.dll libintl3.dll msvcr71.dll pgaevent.dll ssleay32.dll)
add_files_to_zip ${depend_path}/pgsql bin $files

#
# dependencies from the cross compilation environment
#
files=(readline5.dll)
add_files_to_zip $cross_path bin $files

#
# Libgda's files
#
files=(bdb_specs_dsn.xml information_schema.xml mdb_specs_dsn.xml mysql_specs_add_column.xml mysql_specs_create_db.xml mysql_specs_create_index.xml mysql_specs_create_table.xml mysql_specs_create_view.xml mysql_specs_drop_column.xml mysql_specs_drop_db.xml mysql_specs_drop_index.xml mysql_specs_drop_table.xml mysql_specs_drop_view.xml mysql_specs_dsn.xml mysql_specs_rename_table.xml postgres_specs_add_column.xml postgres_specs_create_db.xml postgres_specs_create_index.xml postgres_specs_create_table.xml postgres_specs_create_view.xml postgres_specs_drop_column.xml postgres_specs_drop_db.xml postgres_specs_drop_index.xml postgres_specs_drop_table.xml postgres_specs_drop_view.xml postgres_specs_dsn.xml postgres_specs_rename_table.xml sqlite_specs_add_column.xml sqlite_specs_create_db.xml sqlite_specs_create_index.xml sqlite_specs_create_table.xml sqlite_specs_create_view.xml sqlite_specs_drop_db.xml sqlite_specs_drop_index.xml sqlite_specs_drop_table.xml sqlite_specs_drop_view.xml sqlite_specs_dsn.xml sqlite_specs_rename_table.xml)
add_files_to_zip $prefix share/libgda-4.0 $files

files=(libgda-paramlist.dtd libgda-array.dtd libgda-server-operation.dtd)
add_files_to_zip $prefix share/libgda-4.0/dtd $files

files=(config sales_test.db)
add_files_to_zip $prefix etc/libgda-4.0 $files

files=(gda-sql-4.0.exe gda-test-connection-4.0.exe libgda-4.0-4.dll)
add_files_to_zip $prefix bin $files

files=(libgda-sqlite.dll libgda-postgres.dll libgda-mysql.dll libgda-mdb.dll libgda-bdb.dll)
add_files_to_zip $prefix lib/libgda-4.0/providers $files

#
# The End
#
echo "Archive written to ${archive}"
