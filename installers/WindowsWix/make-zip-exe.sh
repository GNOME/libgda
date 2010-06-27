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
prefix=/local/Win32/Libgda
version=4.1.8



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
# Takes 3 arguments:
# $1 = the archive name
# $2 = the prefix directory where files are located
# $3 = the prefix under $1 where all the files are
#
function add_all_files_to_zip
{
    arch=$1
    dir=$2
    subdir=$3
    pushd $dir >& /dev/null
    echo "Adding file $dir/$subdir/$item"
    zip ${arch} $subdir/* >& /dev/null
    if [ $? -ne 0 ]
    then
	echo "Error (file may noy exist)"
	exit 1
    fi
    popd >& /dev/null
}

#
# dependencies DLLs
#
files=(charset.dll iconv.dll intl.dll libgio-2.*.dll libglib-2.*.dll libgmodule-2.*.dll libgobject-2.*.dll libgthread-2.*.dll libxml2*.dll zlib1.dll libsoup-2.*.dll libgdk_pixbuf-2.*.dll libgdk-win32-2.*.dll libgtk-win32-2.*.dll libatk-1.*.dll libpng12-*.dll libpango-1.*.dll libpangocairo-1.*.dll libpangoft2-1.*.dll libpangowin32-1.*.dll libcairo-2.dll libfontconfig-1.dll libgoocanvas-*.dll libgtksourceview-2.0-0.dll)
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
files=(bdb_specs_dsn.xml information_schema.xml mdb_specs_dsn.xml mysql_specs_add_column.xml mysql_specs_create_db.xml mysql_specs_create_index.xml mysql_specs_create_table.xml mysql_specs_create_view.xml mysql_specs_drop_column.xml mysql_specs_drop_db.xml mysql_specs_drop_index.xml mysql_specs_drop_table.xml mysql_specs_drop_view.xml mysql_specs_dsn.xml mysql_specs_rename_table.xml postgres_specs_add_column.xml postgres_specs_create_db.xml postgres_specs_create_index.xml postgres_specs_create_table.xml postgres_specs_create_view.xml postgres_specs_drop_column.xml postgres_specs_drop_db.xml postgres_specs_drop_index.xml postgres_specs_drop_table.xml postgres_specs_drop_view.xml postgres_specs_dsn.xml postgres_specs_rename_table.xml sqlite_specs_add_column.xml sqlite_specs_create_db.xml sqlite_specs_create_index.xml sqlite_specs_create_table.xml sqlite_specs_create_view.xml sqlite_specs_drop_db.xml sqlite_specs_drop_index.xml sqlite_specs_drop_table.xml sqlite_specs_drop_view.xml sqlite_specs_dsn.xml sqlite_specs_rename_table.xml import_encodings.xml)
add_files_to_zip $archive $prefix share/libgda-4.0 $files

files=(gdaui-generic.png)
add_files_to_zip $archive $prefix share/libgda-4.0/pixmaps $files

#copy some Gnome files to be installed on Windows

cp /usr/share/icons/gnome/16x16/actions/bookmark-new.png $prefix/share/libgda-4.0/icons/hicolor/16x16/actions
cp /usr/share/icons/gnome/22x22/actions/bookmark-new.png $prefix/share/libgda-4.0/icons/hicolor/22x22/actions
cp /usr/share/icons/gnome/24x24/actions/bookmark-new.png $prefix/share/libgda-4.0/icons/hicolor/24x24/actions
cp /usr/share/icons/gnome/32x32/actions/bookmark-new.png $prefix/share/libgda-4.0/icons/hicolor/32x32/actions
cp /usr/share/icons/gnome/16x16/actions/window-new.png $prefix/share/libgda-4.0/icons/hicolor/16x16/actions
cp /usr/share/icons/gnome/22x22/actions/window-new.png $prefix/share/libgda-4.0/icons/hicolor/22x22/actions
cp /usr/share/icons/gnome/24x24/actions/window-new.png $prefix/share/libgda-4.0/icons/hicolor/24x24/actions
cp /usr/share/icons/gnome/32x32/actions/window-new.png $prefix/share/libgda-4.0/icons/hicolor/32x32/actions
mkdir -p $prefix/share/libgda-4.0/icons/hicolor/16x16/apps
mkdir -p $prefix/share/libgda-4.0/icons/hicolor/22x22/apps
mkdir -p $prefix/share/libgda-4.0/icons/hicolor/24x24/apps
mkdir -p $prefix/share/libgda-4.0/icons/hicolor/32x32/apps
cp /usr/share/icons/gnome/16x16/apps/accessories-text-editor.png $prefix/share/libgda-4.0/icons/hicolor/16x16/apps
cp /usr/share/icons/gnome/22x22/apps/accessories-text-editor.png $prefix/share/libgda-4.0/icons/hicolor/22x22/apps
cp /usr/share/icons/gnome/24x24/apps/accessories-text-editor.png $prefix/share/libgda-4.0/icons/hicolor/24x24/apps
cp /usr/share/icons/gnome/32x32/apps/accessories-text-editor.png $prefix/share/libgda-4.0/icons/hicolor/32x32/apps

add_all_files_to_zip $archive $prefix share/libgda-4.0/pixmaps
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/16x16/actions
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/22x22/actions
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/24x24/actions
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/32x32/actions
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/scalable/actions
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/16x16/apps
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/22x22/apps
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/24x24/apps
add_all_files_to_zip $archive $prefix share/libgda-4.0/icons/hicolor/32x32/apps
files=(index.theme)
add_files_to_zip $archive . share/libgda-4.0/icons/hicolor $files


files=(gda-browser-4.0.png gda-control-center-4.0.png)
add_files_to_zip $archive $prefix share/pixmaps $files

files=(gdaui-entry-number.xml gdaui-entry-string.xml)
add_files_to_zip $archive $prefix share/libgda-4.0/ui $files

files=(cnc.js md5.js jquery.js mouseapp_2.js mouseirb_2.js irb.js gda.css gda-print.css irb.css)
add_files_to_zip $archive $prefix share/libgda-4.0/web $files

files=(libgda-paramlist.dtd libgda-array.dtd libgda-server-operation.dtd gdaui-layout.dtd)
add_files_to_zip $archive $prefix share/libgda-4.0/dtd $files

files=(config sales_test.db)
add_files_to_zip $archive $prefix etc/libgda-4.0 $files

files=(gdk-pixbuf.loaders gtk.immodules)
add_files_to_zip $archive_ext $cross_path etc/gtk-2.0 $files

files=(gtkrc)
add_files_to_zip $archive_ext . etc/gtk-2.0 $files

files=(pango.modules)
add_files_to_zip $archive_ext $cross_path etc/pango $files

files=(gda-sql-4.0.exe gda-test-connection-4.0.exe libgda-4.0-4.dll libgda-report-4.0-4.dll libgda-ui-4.0-4.dll gda-browser-4.0.exe gda-control-center-4.0.exe)
add_files_to_zip $archive $prefix bin $files

files=(gspawn-win32-helper.exe)
add_files_to_zip $archive $cross_path bin $files

files=(gdaui-demo-4.0.exe)
add_files_to_zip $archive_dev $prefix bin $files

files=(libgda-sqlite.dll libgda-postgres.dll libgda-mysql.dll libgda-bdb.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/providers $files

files=(gdaui-entry-filesel-spec.xml gdaui-entry-password.xml gdaui-entry-pict-spec.xml gdaui-entry-pict-spec_string.xml libgda-ui-plugins.dll)
add_files_to_zip $archive $prefix lib/libgda-4.0/plugins $files

files=(libpixmap.dll libwimp.dll)
add_files_to_zip $archive_ext $cross_path lib/gtk-2.0/2.10.0/engines $files

files=(libpixbufloader-ani.dll libpixbufloader-bmp.dll libpixbufloader-gif.dll libpixbufloader-ico.dll libpixbufloader-jpeg.dll libpixbufloader-pcx.dll libpixbufloader-png.dll libpixbufloader-pnm.dll libpixbufloader-ras.dll libpixbufloader-tga.dll libpixbufloader-tiff.dll libpixbufloader-wbmp.dll libpixbufloader-xbm.dll libpixbufloader-xpm.dll)
add_files_to_zip $archive_ext $cross_path lib/gtk-2.0/2.10.0/loaders $files

#
# includes
#
files=(gda-attributes-manager.h gda-batch.h gda-binreloc.h gda-blob-op.h gda-column.h gda-config.h gda-connection-event.h gda-connection.h gda-connection-private.h gda-data-access-wrapper.h gda-data-comparator.h gda-data-handler.h gda-data-model-array.h gda-data-model-bdb.h gda-data-model-dir.h gda-data-model-extra.h gda-data-model.h gda-data-model-import.h gda-data-model-iter-extra.h gda-data-model-iter.h gda-data-model-private.h gda-data-proxy.h gda-data-select.h gda-debug-macros.h gda-decl.h gda-easy.h gda-enums.h gda-enum-types.h gda-holder.h gda-lockable.h gda-log.h gda-marshal.h gda-meta-store.h gda-meta-struct.h gda-mutex.h gda-quark-list.h gda-row.h gda-server-operation.h gda-server-provider-extra.h gda-server-provider.h gda-server-provider-private.h gda-set.h gda-statement-extra.h gda-statement.h gda-transaction-status.h gda-transaction-status-private.h gda-util.h gda-value.h gda-xa-transaction.h libgda.h gda-repetitive-statement.h gda-sql-builder.h gda-tree.h gda-tree-manager.h gda-tree-mgr-columns.h gda-tree-mgr-label.h gda-tree-mgr-schemas.h gda-tree-mgr-select.h gda-tree-mgr-tables.h gda-tree-node.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda $files

files=(gda-sqlite-provider.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/sqlite $files

files=(gda-thread-wrapper.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/thread-wrapper $files

files=(gda-handler-bin.h gda-handler-boolean.h gda-handler-numerical.h gda-handler-string.h gda-handler-time.h gda-handler-type.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/handlers $files

files=(gda-report-docbook-document.h gda-report-document.h gda-report-engine.h gda-report-rml-document.h libgda-report.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda-report $files

files=(gda-data-select-priv.h gda-pstmt.h gda-meta-column-types.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/providers-support $files

files=(gda-sql-parser-enum-types.h gda-sql-parser.h gda-sql-statement.h gda-statement-struct-compound.h gda-statement-struct-decl.h gda-statement-struct-delete.h gda-statement-struct.h gda-statement-struct-insert.h gda-statement-struct-parts.h gda-statement-struct-pspec.h gda-statement-struct-select.h gda-statement-struct-trans.h gda-statement-struct-unknown.h gda-statement-struct-update.h gda-statement-struct-util.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/sql-parser $files

files=(gda-vconnection-data-model.h gda-vconnection-hub.h gda-virtual-connection.h gda-virtual-provider.h gda-vprovider-data-model.h gda-vprovider-hub.h libgda-virtual.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda/virtual $files

files=(gdaui-basic-form.h gdaui-cloud.h gdaui-combo.h gdaui-data-entry.h gdaui-data-filter.h gdaui-data-proxy.h gdaui-data-proxy-info.h gdaui-data-selector.h gdaui-data-store.h gdaui-data-widget-filter.h gdaui-data-widget.h gdaui-data-widget-info.h gdaui-decl.h gdaui-easy.h gdaui-enums.h gdaui-enum-types.h gdaui-form.h gdaui-grid.h gdaui-login.h gdaui-plugin.h gdaui-provider-selector.h gdaui-raw-form.h gdaui-raw-grid.h gdaui-server-operation.h gdaui-set.h gdaui-tree-store.h libgda-ui.h gdaui-data-cell-renderer-util.h)
add_files_to_zip $archive_dev $prefix include/libgda-4.0/libgda-ui $files

#
# PC files
#
add_all_files_to_zip $archive_dev $prefix lib/pkgconfig

#
# static libs
#
files=(libgda-4.0.a libgda-4.0.dll.a libgda-4.0.lib libgda-4.0.def libgda-report-4.0.a libgda-report-4.0.dll.a libgda-report-4.0.lib libgda-report-4.0.def libgda-ui-4.0.a libgda-ui-4.0.dll.a libgda-ui-4.0.lib libgda-ui-4.0.def)
add_files_to_zip $archive_dev $prefix lib $files

#
# demo
#
files=(data_model_dir.c ddl_queries.c demo_db.db example_automatic_layout.xml form.c form_data_layout.c form_pict.c form_rw.c grid.c grid_data_layout.c grid_pict.c grid_rw.c linked_grid_form.c linked_model_param.c login.c)
add_files_to_zip $archive_dev $prefix share/libgda-4.0/demo $files

#
# doc
#
add_all_files_to_zip $archive_dev $prefix share/gtk-doc/html/libgda-4.0

#
# translations
#
add_all_files_to_zip $archive $prefix share/locale


#
# The End
#
echo "Archives written to:"
echo "   ${archive}"
echo "   ${archive_dev}"
echo "   ${archive_ext}"
