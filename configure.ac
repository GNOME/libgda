m4_define(major, 4)
m4_define(minor, 2)
m4_define(micro, 0)
m4_define([gda_stable],
	m4_if(m4_eval(minor % 2), [0], [yes], [no]))
AC_CONFIG_MACRO_DIR([m4])
m4_include(m4/introspection.m4)
m4_include(m4/mdbtools.m4)
m4_include(m4/bdb.m4)
m4_include(m4/mysql.m4)

AC_INIT(GNU Data Access, major.minor.micro, gnome-db-list@gnome.org, libgda)
AC_PREREQ(2.59)
AC_CONFIG_SRCDIR(libgda/libgda.h.in)
AM_INIT_AUTOMAKE(1.8 -Wall no-define dist-bzip2)

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_SUBST(GDA_VERSION, major.minor.micro)

# ABI version:
# Careful. Changing these will break ABI, because it is used for the library name and header install path:
# This allows us to do a 3.2 version that is ABI-compatible with 3.0, but with API additions.
AC_SUBST(GDA_ABI_MAJOR_VERSION, 4)
AC_SUBST(GDA_ABI_MINOR_VERSION, 0)
#AC_SUBST(GDA_MICRO_VERSION, micro)
LIBGDA_ABI_VERSION=$GDA_ABI_MAJOR_VERSION.$GDA_ABI_MINOR_VERSION
AC_SUBST(GDA_ABI_VERSION, $LIBGDA_ABI_VERSION)
LIBGDA_ABI_NAME="\\\"libgda-$LIBGDA_ABI_VERSION\\\""

gdasysconfdir="$sysconfdir"
AC_SUBST(gdasysconfdir)

m4_undefine([major])
m4_undefine([minor])
m4_undefine([micro])

dnl required versions of other tools.
m4_define([req_ver_glib],[2.12.11])

#
# Making releases:
# - If interfaces have been changed or added, set GDA_CURRENT += 1 and GDA_AGE += 1, set GDA_REVISION to 0.
# - If binary compatibility has been broken (e.g. removed or changed interfaces), set GDA_CURRENT += 1, GDA_REVISION and GDA_AGE to 0
# - If interfaces is the same as the previous version, set GDA_REVISION += 1
#
GDA_CURRENT=5
GDA_REVISION=0
GDA_AGE=1

AC_SUBST(GDA_CURRENT)
AC_SUBST(GDA_REVISION)
AC_SUBST(GDA_AGE)

IT_PROG_INTLTOOL([0.35.5])
AC_SUBST(INTLTOOL_XML_RULE)
AC_CHECK_SIZEOF(unsigned int,0)
AC_CHECK_SIZEOF(unsigned long int,0)
AC_CHECK_TYPE(uint8_t, unsigned char)

dnl **********************
dnl Cross compilation test
dnl **********************
AM_CONDITIONAL(CROSS_COMPILING, test $cross_compiling = yes)
AC_MSG_CHECKING([for a C compiler for build tools])
if test $cross_compiling = yes; then
  AC_CHECK_PROGS(CC_FOR_BUILD, gcc cc)
else
  CC_FOR_BUILD=$CC
fi
AC_MSG_RESULT([$CC_FOR_BUILD])
AC_SUBST(CC_FOR_BUILD)

AC_MSG_CHECKING([for suffix of executable build tools])
if test $cross_compiling = yes; then
  cat >conftest.c <<\_______EOF
int
main ()
{
  exit (0);
}
_______EOF
  for i in .exe ""; do
    compile="$CC_FOR_BUILD conftest.c -o conftest$i"
    if AC_TRY_EVAL(compile); then
      if (./conftest) 2>&AC_FD_CC; then
	EXEEXT_FOR_BUILD=$i
	break
      fi
    fi
  done
  rm -f conftest*
  if test "${EXEEXT_FOR_BUILD+set}" != set; then
    EXEEXT_FOR_BUILD=
  fi
else
  EXEEXT_FOR_BUILD=$EXEEXT
fi
AC_MSG_RESULT([$EXEEXT_FOR_BUILD])
AC_SUBST(EXEEXT_FOR_BUILD)

dnl ******************************
dnl Checks for programs
dnl ******************************
AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_STDC_HEADERS
AM_PROG_LIBTOOL
AC_PROG_MAKE_SET

dnl -- Initialize docs
GNOME_DOC_INIT([0.9.0],[have_gdu=yes],[have_gdu=no])
if test x$have_gdu != xno; then
   AC_DEFINE(HAVE_GDU, [1], [GdaBrowser documentation is built])
fi
AM_CONDITIONAL(HAVE_GDU, test x$have_gdu != xno)

dnl ******************************
dnl glib-genmarshal
dnl ******************************
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

dnl ******************************
dnl Translations
dnl ******************************
GETTEXT_PACKAGE=libgda-$LIBGDA_ABI_VERSION
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE")
AM_GLIB_GNU_GETTEXT


dnl ******************************
dnl Check for pkg-config version; want >= 0.18 because we need PKG_CHECK_EXISTS
dnl ******************************
PKG_PROG_PKG_CONFIG([0.18])

dnl ******************************
dnl Checks for required libraries
dnl ******************************

PKG_CHECK_MODULES(LIBGDA, [
	glib-2.0	>= req_ver_glib
	gobject-2.0	>= req_ver_glib
	gthread-2.0	>= req_ver_glib
	gmodule-no-export-2.0	>= req_ver_glib
	libxml-2.0
])
TST_CFLAGS="$LIBGDA_CFLAGS"
LIBGDA_CFLAGS="$LIBGDA_CFLAGS -DLIBGDA_ABI_NAME=$LIBGDA_ABI_NAME"


dnl ******************************
dnl Checks for UI extension
dnl ******************************
have_ui=no
have_goocanvas=no
have_graphviz=no
have_sourceview=no
AC_ARG_WITH(ui,
	AS_HELP_STRING([--with-ui], [Enable GTK+ extension and tools]),
	,with_ui=auto)

if test "$with_ui" = "auto" -o "$with_ui" = "yes"
then
	GTK_MODULES="gtk+-2.0 >= 2.12.0"
	PKG_CHECK_MODULES(GTK, $GTK_MODULES, [
		AC_DEFINE(HAVE_UI, [1], [GTK+ support enabled])
		have_ui=yes], [
		if test "$with_ui" = "yes"
		then
			AC_MSG_ERROR([GTK+ support requested but not found.])
		fi
		have_ui=no])
fi
AM_CONDITIONAL(HAVE_UI, test x"$have_ui" = "xyes")
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

AC_ARG_WITH(gtksourceview,
AS_HELP_STRING([--with-gtksourceview], [Enable using GtkSourceView]),
	[with_sourceview=$withval]
	,with_sourceview=auto)

AC_ARG_WITH(goocanvas,
AS_HELP_STRING([--with-goocanvas], [Enable using GooCanvas]),
	[with_goo=$withval]
	,with_goo=auto)

AC_ARG_WITH(graphviz,
AS_HELP_STRING([--with-graphviz], [Enable using Graphviz]),
	[with_graphviz=$withval]
	,with_graphviz=auto)

if test x"$have_ui" = "xyes"
then
	if test "$with_sourceview" = "auto" -o "$with_sourceview" = "yes"
	then
		PKG_CHECK_MODULES(GTKSOURCEVIEW, "gtksourceview-2.0", [
			AC_DEFINE(HAVE_GTKSOURCEVIEW, [1], [GtkSourceView support enabled])
			have_sourceview=yes], [
			if test "$with_sourceview" = "yes"
			then
				AC_MSG_ERROR([GtkSourceview support requested but not found.])
			fi
			have_sourceview=no])
	fi

	if test "$with_goo" = "auto" -o "$with_goo" = "yes"
	then
		PKG_CHECK_MODULES(GOOCANVAS, "goocanvas", [
			AC_DEFINE(HAVE_GOOCANVAS, [1], [GooCanvas support enabled])
			have_goocanvas=yes], [
			if test "$with_goo" = "yes"
			then
				AC_MSG_ERROR([GooCanvas support requested but not found.])
			fi
			have_goocanvas=no])
	fi

	if test "$with_graphviz" = "auto" -o "$with_graphviz" = "yes"
	then
		PKG_CHECK_MODULES(GRAPHVIZ, "libgvc", [
			AC_DEFINE(HAVE_GRAPHVIZ, [1], [Graphviz support enabled])
			have_graphviz=yes], [
			if test "$with_graphviz" = "yes"
			then
				AC_MSG_ERROR([Graphviz support requested but not found.])
			fi
			have_graphviz=no])
	fi
fi

AM_CONDITIONAL(HAVE_GTKSOURCEVIEW, test x"$have_sourceview" = "xyes")
AC_SUBST(GTKSOURCEVIEW_CFLAGS)
AC_SUBST(GTKSOURCEVIEW_LIBS)

AM_CONDITIONAL(HAVE_GOOCANVAS, test x"$have_goocanvas" = "xyes")
AC_SUBST(GOOCANVAS_CFLAGS)
AC_SUBST(GOOCANVAS_LIBS)


AM_CONDITIONAL(HAVE_GRAPHVIZ, test x"$have_graphviz" = "xyes")
AC_SUBST(GRAPHVIZ_CFLAGS)
AC_SUBST(GRAPHVIZ_LIBS)


dnl ******************************
dnl Checks for LibUnique
dnl ******************************
have_unique=no
AC_ARG_WITH(unique,
	AS_HELP_STRING([--with-unique], [Use LibUnique to have only one control center]),
	,with_unique=auto)

if test "$with_unique" = "auto" -o "$with_unique" = "yes"
then
	PKG_CHECK_MODULES(UNIQUE, "unique-1.0", [
		AC_DEFINE(HAVE_UNIQUE, [1], [LibUnique support enabled])
		have_unique=yes], [
		if test "$with_unique" = "yes"
		then
			AC_MSG_ERROR([LibUnique support requested but not found.])
		fi
		have_unique=no])
fi
AM_CONDITIONAL(HAVE_UNIQUE, test x"$have_unique" = "xyes")
AC_SUBST(UNIQUE_CFLAGS)
AC_SUBST(UNIQUE_LIBS)

dnl ******************************
dnl Checks for iso codes
dnl ******************************

ISO_CODES_MODULES="iso-codes"
PKG_CHECK_MODULES(ISO_CODES, $ISO_CODES_MODULES, have_iso_codes=yes, have_iso_codes=no)
if test x"$have_iso_codes" = "xyes"
then
        AC_MSG_CHECKING([whether iso-codes has iso-4217 domain])
        if $PKG_CONFIG --variable=domains iso-codes | grep -q 4217 ; then
                AC_MSG_RESULT([yes])
                AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
        else
                AC_MSG_RESULT([no])
                AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],"")
        fi
else
        AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],"")
fi

dnl ******************************
dnl Check for Operating System
dnl ******************************
dnl linklibext is the shared link library extension, which varies by platform

EXPORT_SYM_REGEX='-export-symbols-regex "^(gda_|fnYM49765777344607__gda).*"'
EXPORT_PROV_SYM_REGEX='-export-symbols-regex "^(plugin_|Java_|g_module).*"'
EXPORT_UI_SYM_REGEX='-export-symbols-regex "^(gdaui_).*"'
AC_MSG_CHECKING([for platform])
platform_win32=no
platform_carbon=no
have_ige=no
linklibext=".so"
case "$host" in
*-cygwin*)
    AC_MSG_RESULT([Win32 - cygwin])
    NO_UNDEFINED='-no-undefined'
    LIBTOOL_EXPORT_OPTIONS=$EXPORT_SYM_REGEX
    LIBTOOL_PROV_EXPORT_OPTIONS=$EXPORT_PROV_SYM_REGEX
    LIBTOOL_UI_EXPORT_OPTIONS=$EXPORT_UI_SYM_REGEX
    linklibext=".dll.a"
    AC_MSG_RESULT([Win32])
    ;;
*-mingw*)
    AC_MSG_RESULT([Win32 - MinGW])
    platform_win32=yes
    AC_DEFINE(USING_MINGW)
    NO_UNDEFINED='-no-undefined'
    LIBTOOL_EXPORT_OPTIONS=
    LIBTOOL_PROV_EXPORT_OPTIONS=
    LIBTOOL_UI_EXPORT_OPTIONS=
    linklibext=".dll"
    AC_CHECK_TOOL(WINDRES, windres, windres)
    AC_SUBST(WINDRES)
    AC_CHECK_TOOL(DLLTOOL, dlltool, dlltool)
    AC_SUBST(DLLTOOL)
    ;;
*-*-darwin*)
    dnl Darwin based distributions (including Mac OS X)
    AC_MSG_RESULT([Mac OS X - carbon])
    platform_carbon=yes
    if test "$enable_binreloc" != "no"
    then
	AC_DEFINE(ENABLE_BINRELOC)
	br_cv_binreloc=yes
    fi
    PKG_CHECK_MODULES(MAC_INTEGRATION, ige-mac-integration, have_ige=yes, have_ige=no)
    if test x"$have_ige" = "xyes"
    then
	AC_DEFINE(HAVE_MAC_INTEGRATION)
	AC_SUBST(MAC_INTEGRATION_CFLAGS)
	AC_SUBST(MAC_INTEGRATION_LIBS)
    fi
    LIBTOOL_EXPORT_OPTIONS=$EXPORT_SYM_REGEX
    LIBTOOL_PROV_EXPORT_OPTIONS=$EXPORT_PROV_SYM_REGEX
    LIBTOOL_UI_EXPORT_OPTIONS=$EXPORT_UI_SYM_REGEX
    AC_DEFINE(HAVE_CARBON)
    LIBGDA_LIBS="$LIBGDA_LIBS -framework Carbon"
  ;;
*)
    AC_MSG_RESULT([Unix])
    NO_UNDEFINED=''
    SOPREFIX='lib'
    AM_BINRELOC
    LIBTOOL_EXPORT_OPTIONS=$EXPORT_SYM_REGEX
    LIBTOOL_PROV_EXPORT_OPTIONS=$EXPORT_PROV_SYM_REGEX
    LIBTOOL_UI_EXPORT_OPTIONS=$EXPORT_UI_SYM_REGEX
    ;;
esac

AM_CONDITIONAL(PLATFORM_WIN32, [test $platform_win32 = yes])
AM_CONDITIONAL(PLATFORM_CARBON, [test $platform_carbon = yes])
AM_CONDITIONAL(HAVE_IGE, [test $have_ige = yes])
AC_SUBST(LIBTOOL_EXPORT_OPTIONS)
AC_SUBST(LIBTOOL_PROV_EXPORT_OPTIONS)
AC_SUBST(LIBTOOL_UI_EXPORT_OPTIONS)
AC_SUBST(NO_UNDEFINED)
AC_SUBST(SOPREFIX)

dnl *******************************************************
dnl Checks for GIO
dnl *******************************************************

GIO_MODULES="gio-2.0 >= 2.16"
PKG_CHECK_MODULES(GIO, $GIO_MODULES, have_gio=yes, have_gio=no)
AM_CONDITIONAL(HAVE_GIO, test x"$have_gio" = "xyes")
if test x"$have_gio" = "xyes"
then
        GIO_CFLAGS="$GIO_CFLAGS -DHAVE_GIO"
else
        AC_MSG_WARN([Libgda depends on Gio to provide notification when configuration files are changed, without Gio support any application using Libgda will have to be restarted when configuration files are changed to take into account those changes]) 
fi
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

dnl ******************************
dnl Checks for libxslt
dnl ******************************

LIBXSLT_MODULES="libxslt"
PKG_CHECK_MODULES(LIBXSLT, $LIBXSLT_MODULES, have_xslt=yes, have_xslt=no)
AM_CONDITIONAL(HAVE_LIBXSLT, test x"$have_xslt" = "xyes")
if test x"$have_xslt" = "xyes"
then
        LIBXSLT_CFLAGS="$LIBXSLT_CFLAGS -DHAVE_LIBXSLT"
fi
AC_SUBST(LIBXSLT_CFLAGS)
AC_SUBST(LIBXSLT_LIBS)

dnl ******************************
dnl Checks for json-glib
dnl ******************************
have_json=no
AC_ARG_ENABLE([json],
	AS_HELP_STRING([--enable-json], [Enable support for JSON, disabled by default]),
	[enable_json=yes],[enable_json=no])

if test "x$enable_json" = xyes
then
    JSON_GLIB_MODULES="json-glib-1.0"
    PKG_CHECK_MODULES(JSON_GLIB, $JSON_GLIB_MODULES,
	AC_DEFINE(HAVE_JSON_GLIB, [], [Defined if JSON support is enabled])
	have_json=yes,
	AC_MSG_ERROR([JSON support requested but not found])
    )
fi
AM_CONDITIONAL(HAVE_JSON_GLIB, [test "$have_json" = "yes"])
AC_SUBST(JSON_GLIB_CFLAGS)
AC_SUBST(JSON_GLIB_LIBS)

dnl *********************************
dnl Check for GObject Introspection
dnl *********************************
GOBJECT_INTROSPECTION_CHECK([0.6.5])

dnl ******************************
dnl Checks for libcrypto
dnl ******************************
AC_ARG_ENABLE([crypto],
	AS_HELP_STRING([--disable-crypto], [Disable SqlCipher database provider (using the libcrypto library)]),
	,
	[enable_crypto=yes])

LIBCRYPTO_MODULES="libcrypto"
PKG_CHECK_MODULES(LIBCRYPTO, $LIBCRYPTO_MODULES,
	AC_DEFINE(HAVE_LIBCRYPTO, [], [Defined if cryptographic support is enabled])
	have_crypto=yes,
	if test "$enable_crypto" = "yes"; then
		AC_MSG_ERROR([libcrypto support requested but not found])
	fi
	have_crypto=no
)
AM_CONDITIONAL(HAVE_LIBCRYPTO, [test "$have_crypto" = "yes"])
AC_SUBST(LIBCRYPTO_CFLAGS)
AC_SUBST(LIBCRYPTO_LIBS)

dnl ******************************
dnl Checks for libgcrypt
dnl ******************************
LIBGCRYPT_CFLAGS=""
LIBGCRYPT_LIBS=""
if test x$platform_win32 != xyes; then
	AC_DEFUN([VERSION_TO_NUMBER],[`$1 | awk 'BEGIN { FS = "."; } { printf "%d", ([$]1 * 1000 + [$]2) * 1000 + [$]3;}'`])
	AC_PATH_PROG(LIBGCRYPT_CONFIG, libgcrypt-config, no)
	if test "$LIBGCRYPT_CONFIG" != "no" ; then
   		LIBGCRYPT_VERSION=`$LIBGCRYPT_CONFIG --version`
		if test VERSION_TO_NUMBER(echo $LIBGCRYPT_VERSION) -lt VERSION_TO_NUMBER(echo "1.1.42") ; then
			LIBGCRYPT_CFLAGS=""
			LIBGCRYPT_LIBS=""
			AM_CONDITIONAL(HAVE_LIBGCRYPT, 0)
			echo "Password entry plugin will not be compiled, install libgcrypt >= 1.1.42 and reconfigure to make available."
      		else
			LIBGCRYPT_CFLAGS=`$LIBGCRYPT_CONFIG $libgcrypt_config_args --cflags`
        		LIBGCRYPT_LIBS=`$LIBGCRYPT_CONFIG $libgcrypt_config_args --libs`
			AM_CONDITIONAL(HAVE_LIBGCRYPT, true)
        	fi
	else
	LIBGCRYPT_CFLAGS=""
      	LIBGCRYPT_LIBS=""
      	AM_CONDITIONAL(HAVE_LIBGCRYPT, 0)
      	echo "Password entry plugin will not be compiled, install libgcrypt and reconfigure to make available."
	fi
else
	AM_CONDITIONAL(HAVE_LIBGCRYPT, false)
fi

AC_SUBST(LIBGCRYPT_CFLAGS)
AC_SUBST(LIBGCRYPT_LIBS)

dnl ******************************
dnl gtk-doc
dnl ******************************

GTK_DOC_CHECK([1.0])

dnl ******************************
dnl Checks for providers
dnl ******************************

AC_ARG_WITH(libdir-name,
    AS_HELP_STRING([--with-libdir-name[=@<:@<dir. name>@:>@]],
                   [Speficy the library name used to locate libraries, relative to the prefix of each install (for example lib64)]),
                   [lib=$withval],
		   [lib=""])

dnl Test for Berkeley DB
BDB_CHECK($lib)

dnl Test for MySQL
MYSQL_CHECK($lib)

dnl Test for PostgreSQL
postgres_req=auto
AC_ARG_WITH(postgres,
[  --with-postgres=<directory>	use PostgreSQL backend in <directory>],[
if test $withval = no
then
	postgres_req=none
elif test $withval = yes
then
	postgres_req=none
	if test $cross_compiling = yes
	then
		AC_MSG_WARN(Can't auto determine PostgreSQL installation in cross-compiling environment)
	else
		AC_MSG_CHECKING(for PostgreSQL files)
		AC_PATH_PROG(POSTGRES_CONFIG, pg_config)
	fi
else
	AC_MSG_CHECKING(for PostgreSQL files in $withval)
	if test $cross_compiling = yes
	then
		postgres_req=$withval	
	else
		AC_PATH_PROG(POSTGRES_CONFIG, pg_config, [], $withval/bin)
	fi	
fi
])
postgresdir=""

if test $postgres_req = auto
then
	AC_MSG_CHECKING(for PostgreSQL files)
	AC_PATH_PROG(POSTGRES_CONFIG, pg_config)
fi

if test x$POSTGRES_CONFIG != x
then
	postgresdir=yes
	AC_MSG_RESULT(found postgres)
	AC_DEFINE(HAVE_POSTGRES, 1, [Have Postgres])
	POSTGRES_CFLAGS=-I`$POSTGRES_CONFIG --includedir`
	POSTGRES_LIBS="-L`$POSTGRES_CONFIG --libdir` -lpq"
else
	if test $cross_compiling = yes
	then
		if test $platform_win32 = yes -a -f $postgres_req/$lib/libpq.lib
		then
			postgresdir=$postgres_req
			postgreslibdir_suffix=
			AC_MSG_RESULT(found postgres)
			AC_DEFINE(HAVE_POSTGRES, 1, [Have Postgres])
			POSTGRES_CFLAGS="-I${postgresdir}/include${postgresdir_suffix}"
			POSTGRES_LIBS="-L${postgresdir}/${lib}${postgreslibdir_suffix} -lpq"
		else
			AC_MSG_WARN(Postgres backend not used)
		fi
	fi
fi
AM_CONDITIONAL(POSTGRES, test x$postgresdir != x)

dnl test for ORACLE
try_oracle=true
AC_ARG_WITH(oracle,
[  --with-oracle=<directory>	use ORACLE backend in <directory>],[
if test $withval = no
then
	try_oracle=false
elif test $withval = yes
then
	dir="/usr/local"
else
	dir=$withval
fi
])
if test $try_oracle = true
then
	AC_MSG_CHECKING(for ORACLE installation)
	for d in $dir $ORACLE_HOME /opt/oracle /usr
	do
		if test -f $d/$lib/libclntsh.so
		then
			AC_MSG_RESULT(found ORACLE in $d)
			oracledir=$d
			oraclelibdir=$lib
			break
		else
			if test -f $d/lib/libclntsh.so
			then
				AC_MSG_RESULT(found ORACLE in $d)
				oracledir=$d
				oraclelibdir="lib"
				break
			fi
		fi
	done
	if test x$oracledir = x
	then
		AC_MSG_WARN(ORACLE backend not used)
	else
		AC_DEFINE(HAVE_ORACLE, 1, [Have ORACLE])
		ORACLE_CFLAGS=" -I$oracledir/include -I$oracledir/include/oracle/client -I$oracledir/rdbms/demo -I${ORACLE_HOME}/rdbms/public -I${ORACLE_HOME}/plsql/public -I$oracledir/network/public"
		ORACLE_LIBS="-L$oracledir/$oraclelibdir -lm -ldl -lclntsh"
	fi
fi

AM_CONDITIONAL(ORACLE, test x$oracledir != x)

dnl test for FireBird
try_firebird=true
AC_ARG_WITH(firebird,
[  --with-firebird=<directory>	use FireBird backend in <directoty>],[
if test $withval = no
then
	try_firebird=false
elif test $withval = yes
then
	dir="/usr/local"
else
	dir=$withval
fi
])
if test $try_firebird = true
then
	AC_MSG_CHECKING(for FireBird installation)
	for d in $dir /usr/local/firebird /usr/local /usr /opt/firebird
	do
		if test -f $d/include/ibase.h
		then
			AC_MSG_RESULT(found FireBird in $d)
			firebirddir=$d
			break
		fi
	done
	if test x$firebirddir = x
	then
		AC_MSG_WARN(FireBird backend not used)
	else
		AC_DEFINE(HAVE_FIREBIRD, 1, [Have FireBird])
		FIREBIRD_CFLAGS="-I${firebirddir}/include"
		if test -f $firebirddir/lib/libfbembed.so
		then
			FIREBIRD_LIBS="-L${firebirddir}/lib -lfbembed"
		else
			if test -f $firebirddir/lib/libfbclient.so
			then
				FIREBIRD_LIBS="-L${firebirddir}/lib -lfbclient"
			else
				FIREBIRD_LIBS="-L${firebirddir}/lib -lgds -ldl -lcrypt"
			fi
		fi
	fi
fi

AM_CONDITIONAL(FIREBIRD, test x$firebirddir != x)

dnl Test for MDB Tools (for MS Access files)
MDBTOOLS_CHECK([$lib])

dnl test for system-installed SQLite
dnl If no SQLite is found or if the SQLite found does not have the sqlite3_table_column_metadata()
dnl function call, then use the embedded version
dnl also, on Win32, always use the embedded version
if test x"$platform_win32" = "xyes"
then
        AM_CONDITIONAL(HAVE_SQLITE, 0)
else
	AC_ARG_ENABLE(system-sqlite,
        [  --enable-system-sqlite  Use SQLite installed on the system [default=no]],
        use_syst_sqlite="Yes", use_syst_sqlite="No")

	if test "x$use_syst_sqlite" = "xYes"; then
		SQLITE_MODULES="sqlite3 >= 3.6.11"
		PKG_CHECK_MODULES(SQLITE, $SQLITE_MODULES, have_sqlite=yes, have_sqlite=no)
		if test x"$have_sqlite" = "xyes"
		then
			AC_CHECK_LIB(sqlite3, sqlite3_table_column_metadata,[sqlite3_api=1], [sqlite3_api=0], $SQLITE_CFLAGS $SQLITE_LIBS)
	
			if test $sqlite3_api = 0
        		then
				AC_MSG_RESULT([Installed SQLite was not compiled with the SQLITE_ENABLE_COLUMN_METADATA, using embedded SQLite])
                		have_sqlite=no
        		else
                		AC_MSG_RESULT(Note: using system installed version of SQLite, meta data for functions will not be available)
				have_sqlite=yes
                		SQLITE_CFLAGS="$SQLITE_CFLAGS -DHAVE_SQLITE"
				SQLITE_PATH=`pkg-config --variable=libdir sqlite3`
                		AC_SUBST(SQLITE_CFLAGS)
                		AC_SUBST(SQLITE_LIBS)
                		AC_SUBST(SQLITE_VERS)
                		AC_SUBST(SQLITE_PATH)
        		fi
		fi
	fi
	AM_CONDITIONAL(HAVE_SQLITE, test x"$have_sqlite" = "xyes")
fi

dnl Test for JAVA and JDBC

## RUN_JAVA(variable for the result, parameters)
## ----------
## runs the java interpreter ${JAVA_PROG} with specified parameters and
## saves the output to the supplied variable. The exit value is ignored.
AC_DEFUN([RUN_JAVA],
[
  acx_java_result=
  if test -z "${JAVA_PROG}"; then
    echo "$as_me:$LINENO: JAVA_PROG is not set, cannot run java $2" >&AS_MESSAGE_LOG_FD
  else
    echo "$as_me:$LINENO: running ${JAVA_PROG} $2" >&AS_MESSAGE_LOG_FD
    acx_java_result=`${JAVA_PROG} $2 2>&AS_MESSAGE_LOG_FD`
    echo "$as_me:$LINENO: output: '$acx_java_result'" >&AS_MESSAGE_LOG_FD
  fi
  $1=$acx_java_result
])

try_java=true
AC_ARG_WITH(java,
[  --with-java=<directory>     use JAVA library in <directory>],[
if test $withval = no
then
	try_java=false
elif test $withval = yes
then
	if test -z "${JAVA_HOME}" ; then
	  JAVA_PATH=${PATH}
	else
  	  JAVA_PATH=${JAVA_HOME}:${JAVA_HOME}/bin:${PATH}
	fi
	JAVA_PATH=${JAVA_PATH}:/usr/java/bin:/usr/jdk/bin:/usr/lib/java/bin:/usr/lib/jdk/bin:/usr/local/java/bin:/usr/local/jdk/bin:/usr/local/lib/java/bin:/usr/local/lib/jdk/bin
else
	JAVA_PATH=$withval
fi
], [
if test -z "${JAVA_HOME}" ; then
	  JAVA_PATH=${PATH}
	else
  	  JAVA_PATH=${JAVA_HOME}:${JAVA_HOME}/bin:${PATH}
	fi
	JAVA_PATH=${JAVA_PATH}:/usr/java/bin:/usr/jdk/bin:/usr/lib/java/bin:/usr/lib/jdk/bin:/usr/local/java/bin:/usr/local/jdk/bin:/usr/local/lib/java/bin:/usr/local/lib/jdk/bin
])

save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"
save_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"
have_java=no
if test $try_java = true
then
	## FIXME: we may want to check for jikes, kaffe and others...
	AC_PATH_PROGS(JAVA_PROG,java,,${JAVA_PATH})
	AC_PATH_PROGS(JAVAC,javac,,${JAVA_PATH})
	AC_PATH_PROGS(JAVAH,javah,,${JAVA_PATH})
	AC_PATH_PROGS(JAR,jar,,${JAVA_PATH})

	have_all_java=yes
	if test -z "$JAVA_PROG"; then have_all_java=no; fi
	if test -z "$JAVAC"; then have_all_java=no; fi
	if test -z "$JAVAH"; then have_all_java=no; fi
	if test -z "$JAR"; then have_all_java=no; fi
	if test ${have_all_java} = no; then
		AC_MSG_WARN([one or more Java tools are missing (JRE is not sufficient)])
    	   	try_java=false
	fi
fi

if test $try_java = true
then
	AC_MSG_CHECKING([whether Java interpreter works])
	try_java=false
	acx_java_works=no
	if test -n "${JAVA_PROG}" ; then
	  RUN_JAVA(acx_jc_result,[-classpath . getsp -test])
	  if test "${acx_jc_result}" = "Test1234OK"; then
	    acx_java_works=yes
	  fi
	  acx_jc_result=
	fi

	if test ${acx_java_works} = yes; then
	  AC_MSG_RESULT([yes])

	  AC_MSG_CHECKING([java compiler version])
	  JTYPE=`$JAVAC -version 2>&1 | grep -o Unrecognized`
	  if test x$JTYPE = xUnrecognized; then
	       # eclipse gjc?
	       JVERSION=ECJ`$JAVAC -v 2>&1 | grep -o '[[0-9]].[[0-9]].[[0-9]] release' | sed -e 's/ release//'`
	  else
	       JTYPE=`$JAVAC -version 2>&1 | grep -o Eclipse`
	       if test x$JTYPE = xEclipse; then
	       	       JVERSION=ECJ`$JAVAC -v 2>&1 | grep -o '[[0-9]].[[0-9]].[[0-9]] release' | sed -e 's/ release//'`
	       else
		       # Sun's java compiler
	       	       JVERSION=JRE`$JAVAC -version 2>&1 | sed -e "1s/javac //" -e "1q"`
	       fi
	  fi
	  case "$JVERSION" in
               JRE1.5.*)
		try_java=true
		JTYPE="Sun JRE 1.5"
		JFLAGS="-Xlint:unchecked -Xlint:deprecation"
		;;
               JRE1.6.*)
		try_java=true
		JTYPE="Sun JRE 1.6"
		JFLAGS="-Xlint:unchecked -Xlint:deprecation"
		;;
               JREgcj-4*)
		try_java=true
		JTYPE="GCJ"
		JFLAGS=""
		;;
               ECJ*) try_java=no;;
               *)
	       ;;
	  esac
	  if test $try_java = true; then
	     AC_MSG_RESULT([JAVA version used is $JTYPE ])

	     AC_MSG_CHECKING([for Java environment])
	     ## retrieve JAVA_HOME from Java itself if not set
	     if test -z "${JAVA_HOME}" ; then
	       RUN_JAVA(JAVA_HOME,[-classpath . getsp java.home])
	     fi

	     ## the availability of JAVA_HOME will tell us whether it's supported
	     if test -z "${JAVA_HOME}" ; then
	       if test x$acx_java_env_msg != xyes; then
	         AC_MSG_RESULT([not found])
	       fi
	     else
	       AC_MSG_RESULT([in ${JAVA_HOME}])

	       case "${host_os}" in
	         darwin*)
	           JAVA_LIBS="-framework JavaVM"
	           JAVA_LD_PATH=
	           ;;
	         *)
	           RUN_JAVA(JAVA_LIBS, [-classpath . getsp -libs])
	           JAVA_LIBS="${JAVA_LIBS} -ljvm"
	           RUN_JAVA(JAVA_LD_PATH, [-classpath . getsp java.library.path])
	           ;;
	       esac
	       ## note that we actually don't test JAVA_LIBS - we hope that the detection
	       ## was correct. We should also test the functionality for javac.
	     fi
	  else
	     AC_MSG_RESULT([SUN Java compiler Version >= 1.5 required, JDBC provider not compiled])
	  fi
	else
	  AC_MSG_RESULT([no])
	  AC_MSG_WARN([Java not found. Please install JDK 1.4 or later, make sure that the binaries are on the PATH and re-try. If that doesn't work, set JAVA_HOME correspondingly.])
	  try_java=false
	fi
fi

JNI_H=
AC_ARG_WITH(jni,[  --with-jni=<directory>     use JNI headers in <directory>],[JNI_H=$withval])

if test $try_java = true
then
	if test x$JNI_H = x
	then
	    AC_CHECK_FILE(${JAVA_HOME}/include/jni.h,
	     [JNI_H="${JAVA_HOME}/include"],
	     [AC_CHECK_FILE(${JAVA_HOME}/jni.h,
	      [JNI_H="${JAVA_HOME}"],
	      [AC_CHECK_FILE(${JAVA_HOME}/../include/jni.h,
	       [JNI_H="${JAVA_HOME}/../include"],
	       [AC_MSG_RESULT([jni headers not found. Please make sure you have a proper JDK installed.])
	      ])
	     ])
	    ])
	    if test x$JNI_H = x
	    then
	        try_java=false
	    fi
	fi
fi

if test $try_java = true
then
	JAVA_CFLAGS="-I${JNI_H}"
	#: ${JAVA_CFLAGS=-D_REENTRANT}

	# Sun's JDK needs jni_md.h in in addition to jni.h and unfortunately it's stored somewhere else ...
	# this should be become more general at some point - so far we're checking linux and solaris only
	# (well, those are presumably the only platforms supported by Sun's JDK and others don't need this
	# at least as of now - 01/2004)
	jac_found_md=no
	for mddir in . linux solaris ppc irix alpha aix hp-ux genunix cygwin win32 freebsd; do
	    AC_CHECK_FILE(${JNI_H}/$mddir/jni_md.h,[JAVA_CFLAGS="${JAVA_CFLAGS} -I${JNI_H}/$mddir" jac_found_md=yes])
	    if test ${jac_found_md} = yes
	    then
	        break
	    fi
	done

	LIBS="${LIBS} ${JAVA_LIBS}"
	CFLAGS="${CFLAGS} ${JAVA_CFLAGS} ${JAVA_CFLAGS}"

	AC_MSG_CHECKING([whether JNI programs can be compiled])
	AC_LINK_IFELSE([
	#include <jni.h>
	int main(void) {
	    jobject o;
	    return 0;
	}
	      ],[AC_MSG_RESULT(yes)],
	      [AC_MSG_ERROR([Cannot compile a simple JNI program. See config.log for details.])])

	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${JAVA_LD_PATH}
	export LD_LIBRARY_PATH

	AC_MSG_CHECKING([whether JNI programs can be run])
	AC_RUN_IFELSE([
	#include <jni.h>
	int main(void) {
	    jobject o;
	    return 0;
	}
	      ],[AC_MSG_RESULT(yes)],
	      [AC_MSG_ERROR([Cannot run a simple JNI program - probably your jvm library is in non-standard location or JVM is unsupported. See config.log for details.])])

	AC_MSG_CHECKING([JNI data types])
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
	#include <jni.h>
	int main(void) {
	  return (sizeof(int)==sizeof(jint) && sizeof(long)==sizeof(long) && sizeof(jbyte)==sizeof(char) && sizeof(jshort)==sizeof(short) && sizeof(jfloat)==sizeof(float) && sizeof(jdouble)==sizeof(double))?0:1;
}
	      ]])],[AC_MSG_RESULT([ok])],[AC_MSG_ERROR([One or more JNI types differ from the corresponding native type. You may need to use non-standard compiler flags or a different compiler in order to fix this.])],[])

	JAVA_CFLAGS="-I${JNI_H}"
	for mddir in . linux solaris ppc irix alpha aix hp-ux genunix cygwin win32 freebsd; do
	    if test -e ${JNI_H}/$mddir/jni_md.h
	    then
		JAVA_CFLAGS="${JAVA_CFLAGS} -I${JNI_H}/$mddir"
		break
	    fi
	done
	
	have_java=yes
	AC_DEFINE(HAVE_JAVA, 1, [Have JAVA])
	AC_SUBST(JAVA_LD_PATH)
	AC_SUBST(JFLAGS)
fi
AM_CONDITIONAL(JAVA, test x$have_java = xyes)
CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"
export LD_LIBRARY_PATH="$save_LD_LIBRARY_PATH"

dnl ************************
dnl Check for libsoup
dnl ************************

AC_ARG_WITH(libsoup,
	AS_HELP_STRING([--with-libsoup], [Enable libsoup support]),
	,with_libsoup=auto)

if test "$with_libsoup" = "auto" -o "$with_libsoup" = "yes"
then
	PKG_CHECK_MODULES(LIBSOUP, libsoup-2.4, [
		AC_DEFINE(HAVE_LIBSOUP, [1], [libsoup support enabled])
		have_libsoup=yes], [
		if test "$with_libsoup" = "yes"
		then
			AC_MSG_ERROR([libsoup support requested but not found.])
		fi
		have_libsoup=no])
fi
AM_CONDITIONAL(LIBSOUP, test "$have_libsoup" = "yes")
AC_SUBST(LIBSOUP_CFLAGS)
AC_SUBST(LIBSOUP_LIBS)


dnl ************************
dnl Check for gnome-keyring
dnl ************************

AC_ARG_WITH(gnome-keyring,
	AS_HELP_STRING([--with-gnome-keyring], [Enable gnome-keyring support]),
	,with_gnome_keyring=auto)

if test "$with_gnome_keyring" = "auto" -o "$with_gnome_keyring" = "yes"
then
	PKG_CHECK_MODULES(GNOME_KEYRING, gnome-keyring-1, [
		AC_DEFINE(HAVE_GNOME_KEYRING, [1], [gnome-keyring support enabled])
		have_gnome_keyring=yes], [
		if test "$with_gnome_keyring" = "yes"
		then
			AC_MSG_ERROR([gnome-keyring support requested but not found.])
		fi
		have_gnome_keyring=no])
fi
AM_CONDITIONAL(GNOME_KEYRING, test "$have_gnome_keyring" = "yes")
AC_SUBST(GNOME_KEYRING_CFLAGS)
AC_SUBST(GNOME_KEYRING_LIBS)


dnl **************************
dnl Check for readline/history
dnl **************************
if test x"$platform_win32" = "xyes"; then
	if test $cross_compiling = yes; then
  		AC_CHECK_LIB(readline, readline, READLINE_LIB='-lreadline', [])
	else
  		AC_CHECK_LIB(readline, readline, READLINE_LIB='-lreadline -lcurses', [], -lcurses)
	fi
else
	AC_CHECK_LIB(ncurses, initscr, curses_lib='-lncurses', curses_lib='')
	if test "x$curses_lib" = "x"; then
		AC_CHECK_LIB(curses, initscr, curses_lib='-lcurses', [])
        fi
	if test "x$curses_lib" != "x"; then
		AC_CHECK_LIB(readline, readline, READLINE_LIB="-lreadline ${curses_lib}", [], $curses_lib)
	fi
fi
if test ! x"$READLINE_LIB" = x ; then
   	AC_DEFINE(HAVE_READLINE)
fi
AM_CONDITIONAL(READLINE_LIB, test ! "x$READLINE_LIB" = "x")
AC_SUBST(READLINE_LIB)

AC_CHECK_LIB(history,add_history,HISTORY_LIB=-lhistory)
if test ! x"$HISTORY_LIB" = x ; then
   	AC_DEFINE(HAVE_HISTORY)
fi
AM_CONDITIONAL(HISTORY_LIB, test ! "x$HISTORY_LIB" = "x")
AC_SUBST(HISTORY_LIB)


AC_SUBST(LIB_PREFIX)
AC_SUBST(LIB_SONAME)
AC_SUBST(LIB_SUFFIX)

dnl
dnl TERMIOS.H
dnl
AC_CHECK_HEADERS([termios.h])

dnl
dnl where to install DTD files
dnl
LIBGDA_DTDDIR=${datadir}/libgda-$LIBGDA_ABI_VERSION/dtd
AC_SUBST(LIBGDA_DTDDIR)


dnl ******************************
dnl Variables
dnl ******************************
AC_SUBST(POSTGRES_LIBS)
AC_SUBST(POSTGRES_CFLAGS)
AC_SUBST(PRIMEBASE_LIBS)
AC_SUBST(PRIMEBASE_CFLAGS)
AC_SUBST(ORACLE_LIBS)
AC_SUBST(ORACLE_CFLAGS)
AC_SUBST(FIREBIRD_LIBS)
AC_SUBST(FIREBIRD_CFLAGS)
AC_SUBST(CAMEL_LIBS)
AC_SUBST(CAMEL_CFLAGS)
AC_SUBST(JAVA_LIBS)
AC_SUBST(JAVA_CFLAGS)

dnl
dnl Check if lib should be build with the debug mode
dnl
AC_ARG_ENABLE(debug,
        [  --enable-debug          debug [default=no]],
        use_debug="Yes", use_debug="No")

if test "x$use_debug" = "xYes"; then
	AC_DEFINE(GDA_DEBUG)
	GDA_DEBUG_FLAGS=-DGDA_DEBUG
	AC_SUBST(GDA_DEBUG_FLAGS)
fi

dnl Disable rebuild of glib-mkenum -generated source code:
AC_ARG_ENABLE(rebuilds, [  --disable-rebuilds      disable all source autogeneration rules],,enable_rebuilds=yes)

# define a MAINT-like variable REBUILD which is set if Perl
# is found, so autogenerated sources can be rebuilt
AC_CHECK_PROGS(PERL, perl5 perl)

REBUILD=\#
if test "x$enable_rebuilds" = "xyes" && \
     test -n "$PERL" && \
     $PERL -e 'exit !($] >= 5.002)' > /dev/null 2>&1 ; then
  REBUILD=
fi
AC_SUBST(REBUILD)

#
# enable setting the Gda SQL console as default version (makes a symlink)
#
AC_ARG_ENABLE(default-binary, [ --enable-default-binary install binaries as default (default=gda_stable)], , enable_default_binary=gda_stable)
AM_CONDITIONAL(DEFAULT_BINARY, test "x$enable_default_binary" = xyes)

AC_SUBST(LIBGDA_CFLAGS)
AC_SUBST(LIBGDA_LIBS)

AC_OUTPUT([
Makefile
libgda.spec
libgda-4.0.pc
libgda-ui-4.0.pc
libgda-report-4.0.pc
libgda-xslt-4.0.pc
po/Makefile.in
libgda/Makefile
libgda/libgda.h
libgda/binreloc/Makefile
libgda/handlers/Makefile
libgda/providers-support/Makefile
libgda/sql-parser/Makefile
libgda/sqlite/Makefile
libgda/sqlite/sqlite-src/Makefile
libgda/sqlite/virtual/Makefile
libgda/thread-wrapper/Makefile
providers/Makefile
providers/reuseable/Makefile
providers/reuseable/postgres/Makefile
providers/reuseable/mysql/Makefile
providers/bdb/Makefile
providers/bdb/libgda-bdb-4.0.pc
providers/bdbsql/Makefile
providers/bdbsql/libgda-bdbsql-4.0.pc
providers/mdb/Makefile
providers/mdb/libgda-mdb-4.0.pc
providers/mysql/Makefile
providers/mysql/libgda-mysql-4.0.pc
providers/oracle/Makefile
providers/oracle/libgda-oracle-4.0.pc
providers/postgres/Makefile
providers/postgres/libgda-postgres-4.0.pc
providers/sqlite/Makefile
providers/sqlite/libgda-sqlite-4.0.pc
providers/jdbc/Makefile
providers/jdbc/libgda-jdbc-4.0.pc
providers/web/Makefile
providers/web/libgda-web-4.0.pc
providers/skel-implementation/Makefile
providers/skel-implementation/capi/Makefile
providers/skel-implementation/capi/libgda-capi-4.0.pc
providers/skel-implementation/models/Makefile
providers/skel-implementation/models/libgda-models-4.0.pc
providers/sqlcipher/Makefile
providers/sqlcipher/libgda-sqlcipher-4.0.pc
libgda-report/Makefile
libgda-report/engine/Makefile
libgda-report/DocBook/Makefile
libgda-report/RML/Makefile
libgda-report/RML/trml2html/Makefile
libgda-report/RML/trml2pdf/Makefile
libgda-xslt/Makefile
libgda-ui/Makefile
libgda-ui/marshallers/Makefile
libgda-ui/internal/Makefile
libgda-ui/data-entries/Makefile
libgda-ui/data-entries/plugins/Makefile
libgda-ui/data/Makefile
libgda-ui/demos/Makefile
libgda-ui/demos/geninclude.pl
control-center/Makefile
control-center/data/Makefile
tools/Makefile
tools/gda-sql-4.0.1:tools/gda-sql.1.in
tools/browser/Makefile
tools/browser/data/Makefile
tools/browser/common/Makefile
tools/browser/schema-browser/Makefile
tools/browser/query-exec/Makefile
tools/browser/data-manager/Makefile
tools/browser/dummy-perspective/Makefile
tools/browser/canvas/Makefile
tools/browser/doc/Makefile
tools/browser/help/Makefile
tools/browser/help/gda-browser.omf
testing/Makefile
tests/Makefile
tests/parser/Makefile
tests/data-models/Makefile
tests/providers/Makefile
tests/value-holders/Makefile
tests/meta-store/Makefile
tests/multi-threading/Makefile
doc/Makefile
doc/C/Makefile
doc/C/libgda.types
doc/C/version.xml
data/Makefile
stamp.h
])


dnl Dirty work to be able to build documentation
chmod -R u+w ${srcdir}/doc/C
cp doc/C/version.xml ${srcdir}/doc/C/version.xml.tmp
mv ${srcdir}/doc/C/version.xml.tmp ${srcdir}/doc/C/version.xml

cp ${srcdir}/doc/C/libgda-sections.txt doc/C/libgda-4.0-sections.txt
echo "" >> doc/C/libgda-4.0-sections.txt
cat ${srcdir}/doc/C/libgda-ui-sections.txt >> doc/C/libgda-4.0-sections.txt
cp doc/C/libgda-4.0-sections.txt ${srcdir}/doc/C/libgda-4.0-sections.txt.tmp
mv ${srcdir}/doc/C/libgda-4.0-sections.txt.tmp ${srcdir}/doc/C/libgda-4.0-sections.txt

cp doc/C/libgda.types doc/C/libgda-4.0.types
if test x$have_ui != xno
then
	echo "" >> doc/C/libgda-4.0.types
	cat ${srcdir}/doc/C/libgda-ui.types >> doc/C/libgda-4.0.types
fi


dnl Print configuration summary
echo ""
echo " Configuration summary for version $GDA_VERSION"
echo "   Installation prefix = $prefix"
echo "   Building GTK+ UI extension: `if test x$have_ui != xno; then echo yes; else echo no; fi`"
echo "   Building Libxslt extension: `if test x$have_xslt != xno; then echo yes; else echo no; fi`"
echo "   Compiled providers:"
echo "      Berkeley DB = $bdb_found"
echo "      Berkeley DB SQL = $bdbsql_found"
dnl echo "      FireBird = `if test x$firebirddir != x; then echo yes; else echo no; fi`"
echo "      MDB (MS Access) = $mdbtools_found"
echo "      MySQL = $mysql_found"
echo "      Oracle = `if test x$oracledir != x; then echo yes; else echo no; fi`"
echo "      PostgreSQL = `if test x$postgresdir != x; then echo yes; else echo no; fi`"
echo "      SQLite = yes `if test x$have_sqlite = xyes; then echo '(from system installation)'; else echo '(embedded)'; fi`"
echo "      SQLCipher = `if test x$have_crypto != xyes; then echo no; else echo yes; fi`"
echo "      JDBC = `if test x$have_java = xyes; then echo "yes (using $JTYPE)"; else echo no; fi`"
echo "      WEB = `if test x$have_libsoup = xyes; then echo yes; else echo no; fi`"
if test x"$br_cv_binreloc" != "xyes" -a x"$platform_win32" != "xyes"
then
       echo "   Binreloc support is disabled: Libgda will not be relocatable. To enable binreloc support re-run with --enable-binreloc (see http://autopackage.org/docs/binreloc for more information)"
fi
echo ""

