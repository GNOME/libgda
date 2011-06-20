#!/bin/bash

#
#
# This script creates a .ZIP file containing a minimum set of files required
# to execute the GDA SQL console.
#
# It also creates sevenal .nsh files meant to be used by the gda-browser.nsi file
# to create a GdaBrowser installer file with NSIS
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
version=4.99.3



#
# no modification below
#
current_dir=`pwd`
archive=${current_dir}/libgda-${version}.zip
archive_dev=${current_dir}/libgda-dev-${version}.zip
archive_ext=${current_dir}/libgda-dep-${version}.zip
nshfiles=(core.nsh prov_bdb.nsh prov_mdb.nsh prov_mysql.nsh prov_oracle.nsh prov_postgresql.nsh prov_sqlite.nsh prov_web.nsh prov_ldap.nsh)

# remove current archive if it exists
rm -f $archive $archive_dev $archive_ext
rm -f *.nsh *.exe

if test "$CLEAN" = "yes"
then
    exit 0
fi

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
	echo "Zipping file $dir/$subdir/$item"
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
    echo "Zipping file: $dir/$subdir/$item"
    zip ${arch} $subdir/* >& /dev/null
    if [ $? -ne 0 ]
    then
	echo "Error (file may noy exist)"
	exit 1
    fi
    popd >& /dev/null
}

# test NSH file is known
function check_nsh_file_ok
{
    file=$1
    found="no"
    for item in ${nshfiles[*]}
    do
	if test $item == "$file"
	then
	    found="yes"
	fi
    done
    if test $found == "no"
    then
	echo "NSH file $file not known"
	exit 1
    fi
}

#
# Takes 3 arguments:
# $1 = the section name, without the .nsh extension
# $2 = the prefix directory where files are located
# $3 = the prefix under $1 where all the files are
# $4 = a list of files physically in $1/$2/
#
function add_files_to_nsh
{
    section=$1
    dir=$2
    subdir=$3
    files=$4

    check_nsh_file_ok $section.nsh

    wsubdir=`echo "${subdir}" | sed -e 's/\//\\\\/g'`
    echo "  SetOutPath \"\$INSTDIR\\${wsubdir}\"" >> $section.nsh
    for item in ${files[*]}
    do
	echo "Adding file in section '$section.nsh': $dir/$subdir/$item"
	echo "  File \"$dir/$subdir/$item\"" >> $section.nsh
    done
}

#
# Takes 3 arguments:
# $1 = the section name, without the .nsh extension
# $2 = the prefix directory where files are located
# $3 = the prefix under $1 where all the files are
#
function add_all_files_to_nsh
{
    section=$1
    dir=$2
    subdir=$3

    check_nsh_file_ok $section.nsh

    wsubdir=`echo "${subdir}" | sed -e 's/\//\\\\/g'`
    echo "  SetOutPath \"\$INSTDIR\\${wsubdir}\"" >> $section.nsh

    echo "Adding all files to section '$section.nsh': $dir/$subdir/*"
    echo "  File \"$dir/$subdir/*\"" >> $section.nsh
}

#
# initialize NSH sections
#
cat > core.nsh <<EOF
Section "GdaBrowser & Libgda" SEC01
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
  SectionIn 1 RO
  CreateDirectory "\$SMPROGRAMS\GdaBrowser"
  CreateShortCut "\$SMPROGRAMS\GdaBrowser\GdaBrowser.lnk" "\$INSTDIR\bin\gda-browser-5.0.exe"
  CreateShortCut "\$DESKTOP\GdaBrowser.lnk" "\$INSTDIR\bin\gda-browser-5.0.exe"
EOF

cat > prov_mysql.nsh <<EOF
Section "MySQL" SEC02
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF


cat > prov_bdb.nsh <<EOF
Section /o "Berkeley Db" SEC03
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_mdb.nsh <<EOF
Section "MS Access" SEC04
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_postgresql.nsh <<EOF
Section "PostgreSQL" SEC05
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_oracle.nsh <<EOF
Section /o "Oracle" SEC06
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_sqlite.nsh <<EOF
Section "SQLite" SEC07
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_web.nsh <<EOF
Section /o "Web" SEC08
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > prov_ldap.nsh <<EOF
Section "Ldap" SEC09
  SetOutPath "\$INSTDIR\bin"
  SetOverwrite try
EOF

cat > config.nsh <<EOF
!define PRODUCT_VERSION "$version"
EOF

cat > uninst.nsh <<EOF
Section Uninstall
  Delete "\$INSTDIR\\\${PRODUCT_NAME}.url"
  Delete "\$INSTDIR\uninst.exe"

  RMDir /r "\$SMPROGRAMS\GdaBrowser"
  Delete "\$DESKTOP\GdaBrowser.lnk"

  RMDir /r "\$INSTDIR\bin"
  RMDir /r "\$INSTDIR\lib"
  RMDir /r "\$INSTDIR\etc"
  RMDir /r "\$INSTDIR\share"
  RMDir "\$INSTDIR"

  DeleteRegKey \${PRODUCT_UNINST_ROOT_KEY} "\${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "\${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
EOF

#
# dependencies DLLs
#
files=(Pathplan.dll ltdl.dll libexpat*.dll libgio-2.*.dll libglib-2.*.dll libgmodule-2.*.dll libgobject-2.*.dll libgthread-2.*.dll libxml2*.dll libsoup-2.*.dll libgdk_pixbuf-2.*.dll libgdk-3-0.dll libgtk-3-0.dll libatk-1.*.dll libpng*.dll libpango-1.*.dll libpangocairo-1.*.dll libpangoft2-1.*.dll libpangowin32-1.*.dll libcairo-2.dll libcairo-gobject-2.dll libfontconfig-1.dll libgoocanvas-*.dll cdt.dll graph.dll gvc.dll libfreetype-6.dll libintl-8.dll libpixman-1-0.dll libjasper-1.dll libjpeg-8.dll libtiff-3.dll)
add_files_to_zip $archive_ext ${depend_path}/gtk bin $files
add_files_to_nsh core ${depend_path}/gtk bin $files

files=(libdb51.dll)
add_files_to_zip $archive_ext ${depend_path}/bdb bin $files
add_files_to_nsh prov_bdb ${depend_path}/bdb bin $files

files=(libmySQL.dll)
add_files_to_zip $archive_ext ${depend_path}/mysql bin $files
add_files_to_nsh prov_mysql ${depend_path}/mysql bin $files

files=(iconv.dll libeay32.dll libiconv-2.dll libpq.dll libxml2.dll libxslt.dll msvcr71.dll ssleay32.dll)
add_files_to_zip $archive_ext ${depend_path}/pgsql bin $files
add_files_to_nsh prov_postgresql ${depend_path}/pgsql bin $files

files=(liblber.dll libldap.dll)
add_files_to_zip $archive_ext ${depend_path}/ldap bin $files
add_files_to_nsh prov_ldap ${depend_path}/ldap bin $files

#
# dependencies from the cross compilation environment
#
#files=(readline5.dll)
#add_files_to_zip $archive $cross_path bin $files

#
# Libgda's files
#
files=(information_schema.xml import_encodings.xml)
add_files_to_zip $archive $prefix share/libgda-5.0 $files
add_files_to_nsh core $prefix share/libgda-5.0 $files

files=(gda-sql.lang)
add_files_to_zip $archive $prefix share/libgda-5.0/language-specs $files
add_files_to_nsh core $prefix share/libgda-5.0/language-specs $files

add_all_files_to_zip $archive_ext $cross_path share/gtksourceview-2.0/language-specs
add_all_files_to_nsh core $cross_path share/gtksourceview-2.0/language-specs

files=(bdb_specs_dsn.xml)
add_files_to_zip $archive $prefix share/libgda-5.0 $files
add_files_to_nsh prov_bdb $prefix share/libgda-5.0 $files

files=(mdb_specs_dsn.xml)
add_files_to_zip $archive $prefix share/libgda-5.0 $files
add_files_to_nsh prov_mdb $prefix share/libgda-5.0 $files

files=(mysql_specs_add_column.xml mysql_specs_create_db.xml mysql_specs_create_index.xml mysql_specs_create_table.xml mysql_specs_create_view.xml mysql_specs_drop_column.xml mysql_specs_drop_db.xml mysql_specs_drop_index.xml mysql_specs_drop_table.xml mysql_specs_drop_view.xml mysql_specs_dsn.xml mysql_specs_rename_table.xml)
add_files_to_zip $archive $prefix share/libgda-5.0 $files
add_files_to_nsh prov_mysql $prefix share/libgda-5.0 $files

files=(postgres_specs_add_column.xml postgres_specs_create_db.xml postgres_specs_create_index.xml postgres_specs_create_table.xml postgres_specs_create_view.xml postgres_specs_drop_column.xml postgres_specs_drop_db.xml postgres_specs_drop_index.xml postgres_specs_drop_table.xml postgres_specs_drop_view.xml postgres_specs_dsn.xml postgres_specs_rename_table.xml)
add_files_to_zip $archive $prefix share/libgda-5.0 $files
add_files_to_nsh prov_postgresql $prefix share/libgda-5.0 $files

files=(sqlite_specs_add_column.xml sqlite_specs_create_db.xml sqlite_specs_create_index.xml sqlite_specs_create_table.xml sqlite_specs_create_view.xml sqlite_specs_drop_db.xml sqlite_specs_drop_index.xml sqlite_specs_drop_table.xml sqlite_specs_drop_view.xml sqlite_specs_dsn.xml sqlite_specs_rename_table.xml)
add_files_to_zip $archive $prefix share/libgda-5.0 $files
add_files_to_nsh prov_sqlite $prefix share/libgda-5.0 $files

files=(web_specs_auth.xml web_specs_dsn.xml)
add_files_to_zip $archive $prefix share/libgda-5.0 $files
add_files_to_nsh prov_web $prefix share/libgda-5.0 $files

files=(oracle_specs_dsn.xml oracle_specs_create_table.xml)
add_files_to_zip $archive $prefix share/libgda-5.0 $files
add_files_to_nsh prov_oracle $prefix share/libgda-5.0 $files

files=(ldap_specs_auth.xml ldap_specs_dsn.xml)
add_files_to_zip $archive $prefix share/libgda-5.0 $files
add_files_to_nsh prov_ldap $prefix share/libgda-5.0 $files

files=(gdaui-generic.png)
add_files_to_zip $archive $prefix share/libgda-5.0/pixmaps $files
add_files_to_nsh core $prefix share/libgda-5.0/pixmaps $files


#
#copy some Gnome files to be installed on Windows
#
cp /usr/share/icons/gnome/16x16/actions/bookmark-new.png $prefix/share/libgda-5.0/icons/hicolor/16x16/actions
cp /usr/share/icons/gnome/22x22/actions/bookmark-new.png $prefix/share/libgda-5.0/icons/hicolor/22x22/actions
cp /usr/share/icons/gnome/24x24/actions/bookmark-new.png $prefix/share/libgda-5.0/icons/hicolor/24x24/actions
cp /usr/share/icons/gnome/32x32/actions/bookmark-new.png $prefix/share/libgda-5.0/icons/hicolor/32x32/actions
cp /usr/share/icons/gnome/16x16/actions/window-new.png $prefix/share/libgda-5.0/icons/hicolor/16x16/actions
cp /usr/share/icons/gnome/22x22/actions/window-new.png $prefix/share/libgda-5.0/icons/hicolor/22x22/actions
cp /usr/share/icons/gnome/24x24/actions/window-new.png $prefix/share/libgda-5.0/icons/hicolor/24x24/actions
cp /usr/share/icons/gnome/32x32/actions/window-new.png $prefix/share/libgda-5.0/icons/hicolor/32x32/actions
mkdir -p $prefix/share/libgda-5.0/icons/hicolor/16x16/apps
mkdir -p $prefix/share/libgda-5.0/icons/hicolor/22x22/apps
mkdir -p $prefix/share/libgda-5.0/icons/hicolor/24x24/apps
mkdir -p $prefix/share/libgda-5.0/icons/hicolor/32x32/apps
cp /usr/share/icons/gnome/16x16/apps/accessories-text-editor.png $prefix/share/libgda-5.0/icons/hicolor/16x16/apps
cp /usr/share/icons/gnome/22x22/apps/accessories-text-editor.png $prefix/share/libgda-5.0/icons/hicolor/22x22/apps
cp /usr/share/icons/gnome/24x24/apps/accessories-text-editor.png $prefix/share/libgda-5.0/icons/hicolor/24x24/apps
cp /usr/share/icons/gnome/32x32/apps/accessories-text-editor.png $prefix/share/libgda-5.0/icons/hicolor/32x32/apps

add_all_files_to_zip $archive $prefix share/libgda-5.0/pixmaps
add_all_files_to_nsh core $prefix share/libgda-5.0/pixmaps
add_all_files_to_zip $archive $prefix share/libgda-5.0/icons/hicolor/16x16/actions
add_all_files_to_nsh core $prefix share/libgda-5.0/icons/hicolor/16x16/actions
add_all_files_to_zip $archive $prefix share/libgda-5.0/icons/hicolor/22x22/actions
add_all_files_to_nsh core $prefix share/libgda-5.0/icons/hicolor/22x22/actions
add_all_files_to_zip $archive $prefix share/libgda-5.0/icons/hicolor/24x24/actions
add_all_files_to_nsh core $prefix share/libgda-5.0/icons/hicolor/24x24/actions
add_all_files_to_zip $archive $prefix share/libgda-5.0/icons/hicolor/32x32/actions
add_all_files_to_nsh core $prefix share/libgda-5.0/icons/hicolor/32x32/actions
add_all_files_to_zip $archive $prefix share/libgda-5.0/icons/hicolor/scalable/actions
add_all_files_to_nsh core $prefix share/libgda-5.0/icons/hicolor/scalable/actions
add_all_files_to_zip $archive $prefix share/libgda-5.0/icons/hicolor/16x16/apps
add_all_files_to_nsh core $prefix share/libgda-5.0/icons/hicolor/16x16/apps
add_all_files_to_zip $archive $prefix share/libgda-5.0/icons/hicolor/22x22/apps
add_all_files_to_nsh core $prefix share/libgda-5.0/icons/hicolor/22x22/apps
add_all_files_to_zip $archive $prefix share/libgda-5.0/icons/hicolor/24x24/apps
add_all_files_to_nsh core $prefix share/libgda-5.0/icons/hicolor/24x24/apps
add_all_files_to_zip $archive $prefix share/libgda-5.0/icons/hicolor/32x32/apps
add_all_files_to_nsh core $prefix share/libgda-5.0/icons/hicolor/32x32/apps

files=(index.theme)
add_files_to_zip $archive . share/libgda-5.0/icons/hicolor $files
add_files_to_nsh core . share/libgda-5.0/icons/hicolor $files

files=(gda-browser-5.0.png)
add_files_to_zip $archive $prefix share/pixmaps $files
add_files_to_nsh core $prefix share/pixmaps $files

files=(gda-control-center.png)
add_files_to_zip $archive $prefix share/libgda-5.0/pixmaps $files
add_files_to_nsh core $prefix share/libgda-5.0/pixmaps $files

files=(gdaui-entry-number.xml gdaui-entry-string.xml)
add_files_to_zip $archive $prefix share/libgda-5.0/ui $files
add_files_to_nsh core $prefix share/libgda-5.0/ui $files

files=(cnc.js md5.js jquery.js mouseapp_2.js mouseirb_2.js irb.js gda.css gda-print.css irb.css)
add_files_to_zip $archive $prefix share/libgda-5.0/web $files
add_files_to_nsh core $prefix share/libgda-5.0/web $files

files=(libgda-paramlist.dtd libgda-array.dtd libgda-server-operation.dtd gdaui-layout.dtd)
add_files_to_zip $archive $prefix share/libgda-5.0/dtd $files
add_files_to_nsh core $prefix share/libgda-5.0/dtd $files

files=(config sales_test.db)
add_files_to_zip $archive $prefix etc/libgda-5.0 $files
add_files_to_nsh core $prefix etc/libgda-5.0 $files

files=(gdk-pixbuf.loaders gtk.immodules)
add_files_to_zip $archive_ext $cross_path etc/gtk-3.0 $files
add_files_to_nsh core $cross_path etc/gtk-3.0 $files

files=(gtkrc)
add_files_to_zip $archive_ext . etc/gtk-3.0 $files
add_files_to_nsh core . etc/gtk-2.0 $files

#files=(pango.modules)
#add_files_to_zip $archive_ext $cross_path etc/pango $files
#add_files_to_nsh core $cross_path etc/pango $files

files=(gda-sql-5.0.exe libgda-5.0-4.dll libgda-report-5.0-4.dll libgda-ui-5.0-4.dll gda-browser-5.0.exe gda-control-center-5.0.exe)
add_files_to_zip $archive $prefix bin $files
add_files_to_nsh core $prefix bin $files

files=(gda-test-connection-5.0.exe)
add_files_to_zip $archive $prefix bin $files

files=(gspawn-win32-helper.exe)
add_files_to_zip $archive $cross_path bin $files
add_files_to_nsh core $cross_path bin $files

files=(gdaui-demo-5.0.exe)
add_files_to_zip $archive_dev $prefix bin $files

files=(libgda-bdb.dll)
add_files_to_zip $archive $prefix lib/libgda-5.0/providers $files
add_files_to_nsh prov_bdb $prefix lib/libgda-5.0/providers $files

files=(libgda-mdb.dll)
add_files_to_zip $archive $prefix lib/libgda-5.0/providers $files
add_files_to_nsh prov_mdb $prefix lib/libgda-5.0/providers $files

files=(libgda-mysql.dll)
add_files_to_zip $archive $prefix lib/libgda-5.0/providers $files
add_files_to_nsh prov_mysql $prefix lib/libgda-5.0/providers $files

files=(libgda-postgres.dll)
add_files_to_zip $archive $prefix lib/libgda-5.0/providers $files
add_files_to_nsh prov_postgresql $prefix lib/libgda-5.0/providers $files

files=(libgda-sqlite.dll)
add_files_to_zip $archive $prefix lib/libgda-5.0/providers $files
add_files_to_nsh prov_sqlite $prefix lib/libgda-5.0/providers $files

files=(libgda-web.dll)
add_files_to_zip $archive $prefix lib/libgda-5.0/providers $files
add_files_to_nsh prov_web $prefix lib/libgda-5.0/providers $files

files=(libgda-ldap.dll)
add_files_to_zip $archive $prefix lib/libgda-5.0/providers $files
add_files_to_nsh prov_ldap $prefix lib/libgda-5.0/providers $files

files=(libgda-oracle.dll)
add_files_to_zip $archive $prefix lib/libgda-5.0/providers $files
add_files_to_nsh prov_oracle $prefix lib/libgda-5.0/providers $files

files=(gdaui-entry-filesel-spec.xml gdaui-entry-password.xml gdaui-entry-pict-spec.xml gdaui-entry-pict-spec_string.xml libgda-ui-plugins.dll)
add_files_to_zip $archive $prefix lib/libgda-5.0/plugins $files
add_files_to_nsh core $prefix lib/libgda-5.0/plugins $files

files=(libwimp.dll)
add_files_to_zip $archive_ext $cross_path lib/gtk-3.0/3.0.0/engines $files
add_files_to_nsh core $cross_path lib/gtk-3.0/3.0.0/engines $files

#
# includes
#
files=(gda-attributes-manager.h gda-batch.h gda-binreloc.h gda-blob-op.h gda-column.h gda-config.h gda-connection-event.h gda-connection.h gda-connection-private.h gda-data-access-wrapper.h gda-data-comparator.h gda-data-handler.h gda-data-model-array.h gda-data-model-bdb.h gda-data-model-dir.h gda-data-model-extra.h gda-data-model.h gda-data-model-import.h gda-data-model-iter-extra.h gda-data-model-iter.h gda-data-model-private.h gda-data-proxy.h gda-data-select.h gda-debug-macros.h gda-decl.h gda-enums.h gda-enum-types.h gda-holder.h gda-lockable.h gda-log.h gda-marshal.h gda-meta-store.h gda-meta-struct.h gda-mutex.h gda-quark-list.h gda-row.h gda-server-operation.h gda-server-provider-extra.h gda-server-provider.h gda-server-provider-private.h gda-set.h gda-statement-extra.h gda-statement.h gda-transaction-status.h gda-transaction-status-private.h gda-util.h gda-value.h gda-xa-transaction.h libgda.h libgda-global-variables.h gda-repetitive-statement.h gda-sql-builder.h gda-tree.h gda-tree-manager.h gda-tree-mgr-columns.h gda-tree-mgr-label.h gda-tree-mgr-schemas.h gda-tree-mgr-select.h gda-tree-mgr-tables.h gda-tree-node.h)
add_files_to_zip $archive_dev $prefix include/libgda-5.0/libgda $files

files=(gda-sqlite-provider.h)
add_files_to_zip $archive_dev $prefix include/libgda-5.0/libgda/sqlite $files

files=(gda-thread-wrapper.h)
add_files_to_zip $archive_dev $prefix include/libgda-5.0/libgda/thread-wrapper $files

files=(gda-handler-bin.h gda-handler-boolean.h gda-handler-numerical.h gda-handler-string.h gda-handler-time.h gda-handler-type.h)
add_files_to_zip $archive_dev $prefix include/libgda-5.0/libgda/handlers $files

files=(gda-report-docbook-document.h gda-report-document.h gda-report-engine.h gda-report-rml-document.h libgda-report.h)
add_files_to_zip $archive_dev $prefix include/libgda-5.0/libgda-report $files

files=(gda-data-select-priv.h gda-pstmt.h gda-meta-column-types.h)
add_files_to_zip $archive_dev $prefix include/libgda-5.0/libgda/providers-support $files

files=(gda-sql-parser-enum-types.h gda-sql-parser.h gda-sql-statement.h gda-statement-struct-compound.h gda-statement-struct-decl.h gda-statement-struct-delete.h gda-statement-struct.h gda-statement-struct-insert.h gda-statement-struct-parts.h gda-statement-struct-pspec.h gda-statement-struct-select.h gda-statement-struct-trans.h gda-statement-struct-unknown.h gda-statement-struct-update.h gda-statement-struct-util.h)
add_files_to_zip $archive_dev $prefix include/libgda-5.0/libgda/sql-parser $files

files=(gda-vconnection-data-model.h gda-vconnection-hub.h gda-virtual-connection.h gda-virtual-provider.h gda-vprovider-data-model.h gda-vprovider-hub.h libgda-virtual.h)
add_files_to_zip $archive_dev $prefix include/libgda-5.0/libgda/virtual $files

files=(gdaui-basic-form.h gdaui-data-entry.h gdaui-data-selector.h gdaui-enums.h gdaui-login.h gdaui-raw-grid.h gdaui-cloud.h gdaui-data-filter.h gdaui-data-store.h gdaui-enum-types.h gdaui-plugin.h gdaui-server-operation.h gdaui-combo.h gdaui-data-proxy.h gdaui-decl.h gdaui-form.h gdaui-provider-selector.h gdaui-tree-store.h gdaui-data-cell-renderer-util.h gdaui-data-proxy-info.h gdaui-easy.h gdaui-grid.h gdaui-raw-form.h gdaui-rt-editor.h libgda-ui.h)
add_files_to_zip $archive_dev $prefix include/libgda-5.0/libgda-ui $files

#
# PC files
#
add_all_files_to_zip $archive_dev $prefix lib/pkgconfig

#
# static libs
#
files=(libgda-5.0.dll.a libgda-5.0.lib libgda-5.0.def libgda-report-5.0.dll.a libgda-report-5.0.lib libgda-report-5.0.def libgda-ui-5.0.dll.a libgda-ui-5.0.lib libgda-ui-5.0.def)
add_files_to_zip $archive_dev $prefix lib $files

#
# demo
#
files=(basic_form.c cloud.c combo.c custom_layout.xml data_model_dir.c ddl_queries.c demo_db.db form.c form_data_layout.c form_pict.c form_rw.c grid.c grid_data_layout.c grid_pict.c grid_rw.c linked_grid_form.c linked_model_param.c login.c provider_sel.c tree.c)
add_files_to_zip $archive_dev $prefix share/libgda-5.0/demo $files

#
# doc
#
#add_all_files_to_zip $archive_dev $prefix share/gtk-doc/html/libgda-5.0

#
# translations
#
add_all_files_to_zip $archive $prefix share/locale

#
# end NSH files
#
for item in ${nshfiles[*]}
do
    echo "SectionEnd" >> $item
done

#
# The End
#
echo "Archives written to:"
echo "   ${archive}"
echo "   ${archive_dev}"
echo "   ${archive_ext}"
echo "Generate Windows installer using 'makensis gda-browser.nsi'"
