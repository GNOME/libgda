#!/bin/bash

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

cross_path=/local/Win32/gtk
depend_path=/local/Win32
prefix=/local/Win32/gtk
version=4.0.12



#
# no modification below
#
current_dir=`pwd`
archive=${current_dir}/libgda-${version}.zip
archive_dev=${current_dir}/libgda-dev-${version}.zip
archive_ext=${current_dir}/libgda-dep-${version}.zip

# remove current archive if it exists
rm -f $archive $archive_dev $archive_ext

#
# Takes 3 arguments:
# $1 = the archive name
# $2 = the prefix directory where files are located
# $3 = the prefix under $1 where all the files are
# $4 = a list of files physically in $1/$2/
#
function add_files_to_zip
{
    arch=$1
    dir=$2
    subdir=$3
    files=$4
    pushd $dir >& /dev/null
    for item in ${files[*]}
    do
	echo "Adding file $dir/$subdir/$item"
	zip ${arch} $subdir/$item >& /dev/null
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
files=(charset.dll iconv.dll intl.dll libgio-2.0-0.dll libglib-2.0-0.dll libgmodule-2.0-0.dll libgobject-2.0-0.dll libgthread-2.0-0.dll libxml2.dll zlib1.dll libsoup*.dll)
add_files_to_zip $archive_ext ${depend_path}/gtk bin $files

files=(libdb47.dll msvcp80.dll msvcr80.dll)
add_files_to_zip $archive_ext ${depend_path}/bdb bin $files

files=(libmdb-0.dll)
add_files_to_zip $archive_ext ${depend_path}/mdb bin $files

files=(libmySQL.dll)
add_files_to_zip $archive_ext ${depend_path}/mysql bin $files

files=(libpq.dll comerr32.dll gssapi32.dll k5sprt32.dll krb5_32.dll libeay32.dll libiconv2.dll libintl3.dll msvcr71.dll pgaevent.dll ssleay32.dll)
add_files_to_zip $archive_ext ${depend_path}/pgsql bin $files

#
# dependencies from the cross compilation environment
#
files=(readline5.dll)
add_files_to_zip $archive $cross_path bin $files

#
# Libgda's files
#
files=(bdb_specs_dsn.xml information_schema.xml mdb_specs_dsn.xml mysql_specs_add_column.xml mysql_specs_create_db.xml mysql_specs_create_index.xml mysql_specs_create_table.xml mysql_specs_create_view.xml mysql_specs_drop_column.xml mysql_specs_drop_db.xml mysql_specs_drop_index.xml mysql_specs_drop_table.xml mysql_specs_drop_view.xml mysql_specs_dsn.xml mysql_specs_rename_table.xml postgres_specs_add_column.xml postgres_specs_create_db.xml postgres_specs_create_index.xml postgres_specs_create_table.xml postgres_specs_create_view.xml postgres_specs_drop_column.xml postgres_specs_drop_db.xml postgres_specs_drop_index.xml postgres_specs_drop_table.xml postgres_specs_drop_view.xml postgres_specs_dsn.xml postgres_specs_rename_table.xml sqlite_specs_add_column.xml sqlite_specs_create_db.xml sqlite_specs_create_index.xml sqlite_specs_create_table.xml sqlite_specs_create_view.xml sqlite_specs_drop_db.xml sqlite_specs_drop_index.xml sqlite_specs_drop_table.xml sqlite_specs_drop_view.xml sqlite_specs_dsn.xml sqlite_specs_rename_table.xml jdbc_specs_dsn.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files

files=(cnc.js md5.js jquery.js mouseapp_2.js mouseirb_2.js irb.js gda.css gda-print.css irb.css)
add_files_to_zip $archive $prefix share/libgda-4.0/web $files

files=(libgda-paramlist.dtd libgda-array.dtd libgda-server-operation.dtd)
add_files_to_zip $archive $prefix share/libgda-4.0/dtd $files

files=(config sales_test.db)
add_files_to_zip $archive $prefix etc/libgda-4.0 $files

files=(gda-sql-4.0.exe gda-test-connection-4.0.exe libgda-4.0-4.dll libgda-report-4.0-4.dll)
add_files_to_zip $archive $prefix bin $files

files=(libgda-sqlite.dll libgda-postgres.dll libgda-mysql.dll libgda-mdb.dll libgda-bdb.dll gdaprovider-4.0.jar libgda-jdbc.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/providers $files

#
# includes
#
files=(gda-attributes-manager.h gda-batch.h gda-binreloc.h gda-blob-op.h gda-column.h gda-config.h gda-connection-event.h gda-connection.h gda-connection-private.h gda-data-access-wrapper.h gda-data-comparator.h gda-data-handler.h gda-data-model-array.h gda-data-model-bdb.h gda-data-model-dir.h gda-data-model-extra.h gda-data-model.h gda-data-model-import.h gda-data-model-iter-extra.h gda-data-model-iter.h gda-data-model-private.h gda-data-proxy.h gda-data-select.h gda-debug-macros.h gda-decl.h gda-easy.h gda-enums.h gda-enum-types.h gda-error.h gda-holder.h gda-lockable.h gda-log.h gda-marshal.h gda-meta-store.h gda-meta-struct.h gda-mutex.h gda-quark-list.h gda-row.h gda-server-operation.h gda-server-provider-extra.h gda-server-provider.h gda-server-provider-private.h gda-set.h gda-statement-extra.h gda-statement.h gda-transaction-status.h gda-transaction-status-private.h gda-util.h gda-value.h gda-xa-transaction.h libgda.h libgda-global-variables.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda $files

files=(gda-sqlite-provider.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/sqlite $files

files=(gda-handler-bin.h gda-handler-boolean.h gda-handler-numerical.h gda-handler-string.h gda-handler-time.h gda-handler-type.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/handlers $files

files=(gda-report-docbook-document.h gda-report-document.h gda-report-engine.h gda-report-rml-document.h libgda-report.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda-report $files

files=(gda-data-select-priv.h gda-pstmt.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/providers-support $files

files=(gda-sql-parser-enum-types.h gda-sql-parser.h gda-sql-statement.h gda-statement-struct-compound.h gda-statement-struct-decl.h gda-statement-struct-delete.h gda-statement-struct.h gda-statement-struct-insert.h gda-statement-struct-parts.h gda-statement-struct-pspec.h gda-statement-struct-select.h gda-statement-struct-trans.h gda-statement-struct-unknown.h gda-statement-struct-update.h gda-statement-struct-util.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/sql-parser $files

files=(gda-vconnection-data-model.h gda-vconnection-hub.h gda-virtual-connection.h gda-virtual-provider.h gda-vprovider-data-model.h gda-vprovider-hub.h libgda-virtual.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/virtual $files

#
# PC files
#
files=(libgda-4.0.pc libgda-bdb-4.0.pc libgda-mdb-4.0.pc libgda-mysql-4.0.pc libgda-postgres-4.0.pc libgda-report-4.0.pc libgda-sqlite-4.0.pc libgda-xslt-4.0.pc)
add_files_to_zip $archive_dev $prefix lib/pkgconfig $files

#
# static libs
#
files=(libgda-4.0.a libgda-4.0.dll.a libgda-4.0.lib libgda-4.0.def libgda-report-4.0.a libgda-report-4.0.dll.a libgda-report-4.0.lib libgda-report-4.0.def)
add_files_to_zip $archive_dev $prefix lib $files

#
# doc
#
files=(architecture.html architecture.png ch05s03.html ch06s02.html ch06s03.html ch07s02.html ch07s03.html ch07s06.html ch09s02.html ch09s06.html ch09s07.html ch09s09.html ch09s10.html ch09s11.html ch10.html ch17.html ch25.html ch28.html ch29s02.html ch29s03.html ch30s02.html ch31s02.html ch31s03.html ch33s02.html ch33s03.html ch33s04.html ch33s05.html ch33s06.html ch33s07.html ch33s08.html ch33s10.html compiling.html connection.html connections.html data_conv.html data-model.html data_models.html DataModels.png data_proxy1.png data_proxy2.png data_proxy3.png data_proxy4.png data_proxy5.png data-select.html data_validation_holder.png data_validation_proxy.png data_validation_set.png ddl_example.html fdl.html fdl-section10.html fdl-section1.html fdl-section2.html fdl-section3.html fdl-section4.html fdl-section5.html fdl-section6.html fdl-section7.html fdl-section8.html fdl-section9.html fdl-using.html features.html GdaBatch.html GdaBlobOp.html GdaColumn.html GdaConnectionEvent.html GdaConnection.html GdaDataAccessWrapper.html GdaDataModelArray.html GdaDataModelBdb.html GdaDataModelDir.html GdaDataModel.html GdaDataModelImport.html GdaDataModelIter.html GdaDataModelIter.png GdaDataProxy.html GdaDataSelect.html gda-dict.html GdaHandlerBin.html GdaHandlerBoolean.html GdaHandlerNumerical.html GdaHandlerString.html GdaHandlerTime.html GdaHandlerType.html GdaHolder.html GdaLockable.html GdaMetaStoreCustomData.html GdaMetaStore.html GdaMetaStruct.html GdaPStmt.html GdaReportDocbookDocument.html GdaReportDocument.html GdaReportEngine.html GdaReportRmlDocument.html GdaRow.html GdaServerOperation.html GdaServerProvider.html GdaSet.html gda-sql-graph.png gda-sql.html gda-sql-manual-icommands.html gda-sql-manual-open.html gda-sql-manual-run.html GdaSqlParser.html GdaStatement.html GdaThreader.html GdaTransactionStatus.html GdaVconnectionDataModel.html GdaVconnectionHub.html GdaVirtualConnection.html GdaVirtualProvider.html GdaVproviderDataModel.html GdaVproviderHub.html getting_started.html home.png howto-exec-non-select.html howto-exec-select.html howto.html howto-meta1.html howto-modify-select.html index.html index.sgml information_schema.html information_schema.png init_config.html installation-configuring.html installation.html installation-installing.html introduction.html i_s_data_types.png ix01.html left.png libgda-40-Configuration.html libgda-40-Convenience-functions.html libgda-40-Default-Data-handlers.html libgda-4.0.devhelp libgda-4.0.devhelp2 libgda-40-GdaDataComparator.html libgda-40-GdaMutex.html libgda-40-GdaSqlStatement.html libgda-40-Gda-Value.html libgda-40-GdaXaTransaction.html libgda-40-Libgda-Initialization.html libgda-40-Logging.html libgda-40-Misc-API.html libgda-40-Quark-lists.html libgda-40-Subclassing-GdaDataSelect.html libgda-40-Utility-functions.html libgda-list-server-op.html libgda-provider-blobop.html libgda-provider-class.html libgda-provider-pack.html libgda-provider-recordset.html libgda-reports-introduction.html libgda-sql.html libgda-tools-introduction.html libgda-tools-list-config.html libgda-tools-test-connection.html libgda-xslt-api.html libgda-xslt-introduction.html limitations.html limitations_mysql.html limitations_oracle.html limitations_postgres.html limitations_sqlite.html main_example.html managing-errors.html MetaStore1.png MetaStore2.png migration-1.html migration-2-dict.html migration-2-exec.html migration-2.html misc.html other_examples.html part_begin.html part_index.html part_libgda_api.html part_libgda-reports.html part_libgda-xslt.html part_providers.html parts.png part_tools.html prov-metadata.html psupport.html right.png stmt-compound.png stmt-insert1.png stmt-insert2.png stmt-select.png stmt-unknown.png stmt-update.png style.css transactions.html up.png virtual_connection.html writable_data_model.png)
add_files_to_zip $archive_dev $prefix share/gtk-doc/html/libgda-4.0 $files

#
# translations
#
files=(ga/LC_MESSAGES/libgda-4.0.mo nb/LC_MESSAGES/libgda-4.0.mo gl/LC_MESSAGES/libgda-4.0.mo tr/LC_MESSAGES/libgda-4.0.mo ru/LC_MESSAGES/libgda-4.0.mo ne/LC_MESSAGES/libgda-4.0.mo fi/LC_MESSAGES/libgda-4.0.mo sk/LC_MESSAGES/libgda-4.0.mo sl/LC_MESSAGES/libgda-4.0.mo lt/LC_MESSAGES/libgda-4.0.mo el/LC_MESSAGES/libgda-4.0.mo zh_CN/LC_MESSAGES/libgda-4.0.mo eu/LC_MESSAGES/libgda-4.0.mo ml/LC_MESSAGES/libgda-4.0.mo ms/LC_MESSAGES/libgda-4.0.mo sr@Latn/LC_MESSAGES/libgda-4.0.mo mk/LC_MESSAGES/libgda-4.0.mo fr/LC_MESSAGES/libgda-4.0.mo pl/LC_MESSAGES/libgda-4.0.mo pa/LC_MESSAGES/libgda-4.0.mo ko/LC_MESSAGES/libgda-4.0.mo vi/LC_MESSAGES/libgda-4.0.mo da/LC_MESSAGES/libgda-4.0.mo oc/LC_MESSAGES/libgda-4.0.mo en_GB/LC_MESSAGES/libgda-4.0.mo sr/LC_MESSAGES/libgda-4.0.mo es/LC_MESSAGES/libgda-4.0.mo zh_TW/LC_MESSAGES/libgda-4.0.mo ar/LC_MESSAGES/libgda-4.0.mo nl/LC_MESSAGES/libgda-4.0.mo it/LC_MESSAGES/libgda-4.0.mo en_CA/LC_MESSAGES/libgda-4.0.mo rw/LC_MESSAGES/libgda-4.0.mo hr/LC_MESSAGES/libgda-4.0.mo az/LC_MESSAGES/libgda-4.0.mo ca/LC_MESSAGES/libgda-4.0.mo hu/LC_MESSAGES/libgda-4.0.mo pt/LC_MESSAGES/libgda-4.0.mo uk/LC_MESSAGES/libgda-4.0.mo ja/LC_MESSAGES/libgda-4.0.mo zh_HK/LC_MESSAGES/libgda-4.0.mo sq/LC_MESSAGES/libgda-4.0.mo sv/LC_MESSAGES/libgda-4.0.mo dz/LC_MESSAGES/libgda-4.0.mo fa/LC_MESSAGES/libgda-4.0.mo de/LC_MESSAGES/libgda-4.0.mo cs/LC_MESSAGES/libgda-4.0.mo pt_BR/LC_MESSAGES/libgda-4.0.mo)
add_files_to_zip $archive $prefix share/locale $files


#
# The End
#
echo "Archives written to:"
echo "   ${archive}"
echo "   ${archive_dev}"
echo "   ${archive_ext}"
