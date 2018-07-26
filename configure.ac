m4_define(major, 5)
m4_define(minor, 99)
m4_define(micro, 0)
m4_define([gda_stable],
	m4_if(m4_eval(minor % 2), [0], [yes], [no]))
AC_CONFIG_MACRO_DIR([m4])
m4_include(m4/vapigen.m4)
m4_include(m4/dk-warn.m4)
m4_include(m4/mdbtools.m4)
m4_include(m4/bdb.m4)
m4_include(m4/mysql.m4)
m4_include(m4/postgresql.m4)
m4_include(m4/firebird.m4)
m4_include(m4/oracle.m4)
m4_include(m4/java.m4)
m4_include(m4/ldap.m4)
m4_include(m4/misc.m4)

AC_INIT([GNU Data Access],[major.minor.micro],[https://bugzilla.gnome.org/enter_bug.cgi?product=libgda],[libgda],[http://www.gnome-db.org])
AC_PREREQ([2.68])
AC_CONFIG_SRCDIR(libgda/libgda.h.in)
AM_INIT_AUTOMAKE([1.11.1 -Wall no-define no-dist-gzip dist-xz])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl http://people.gnome.org/~walters/docs/build-api.txt
echo \#buildapi-variable-no-builddir >/dev/null

AC_SUBST(GDA_VERSION, major.minor.micro)

# ABI version:
# Careful. Changing these will break ABI, because it is used for the library name and header install path:
# This allows us to do a 3.2 version that is ABI-compatible with 3.0, but with API additions.
AC_SUBST(GDA_ABI_MAJOR_VERSION, 6)
AC_SUBST(GDA_ABI_MINOR_VERSION, 0)
#AC_SUBST(GDA_MICRO_VERSION, micro)
LIBGDA_ABI_VERSION=$GDA_ABI_MAJOR_VERSION.$GDA_ABI_MINOR_VERSION
AC_SUBST(GDA_ABI_VERSION, $LIBGDA_ABI_VERSION)
LIBGDA_ABI_NAME="\\\"libgda-$LIBGDA_ABI_VERSION\\\""

API_VERSION=6.0
AC_SUBST(GDA_API_VERSION, API_VERSION)

gdasysconfdir="$sysconfdir"
AC_SUBST(gdasysconfdir)

m4_undefine([major])
m4_undefine([minor])
m4_undefine([micro])

dnl required versions of other tools.
m4_define([req_ver_glib],[2.38.0])

#
# Making releases:
# - If interfaces have been changed or added, set GDA_CURRENT += 1 and GDA_AGE += 1, set GDA_REVISION to 0.
# - If binary compatibility has been broken (e.g. removed or changed interfaces), set GDA_CURRENT += 1, GDA_REVISION and GDA_AGE to 0
# - If interfaces is the same as the previous version, set GDA_REVISION += 1
#
GDA_CURRENT=5
GDA_REVISION=1
GDA_AGE=1

AC_SUBST(GDA_CURRENT)
AC_SUBST(GDA_REVISION)
AC_SUBST(GDA_AGE)

IT_PROG_INTLTOOL([0.40.6])
AC_SUBST(INTLTOOL_XML_RULE)
AC_CHECK_SIZEOF(unsigned int,0)
AC_CHECK_SIZEOF(unsigned long int,0)
AC_CHECK_SIZEOF([int *],0)
AC_CHECK_TYPE(uint8_t, unsigned char)
AC_CHECK_FUNCS(localtime_r localtime_s)
AC_CHECK_HEADER(sys/mman.h,
    AC_CHECK_FUNC(mlock, [AC_DEFINE(USE_MLOCK, 1, [Use POSIX memory locking])]))
AC_CHECK_FUNC(fork, [AC_DEFINE(HAVE_FORK, 1, [Have the fork() function])])
CHECK_FD_CLOEXEC([AC_DEFINE(
                  [HAVE_FD_CLOEXEC],
                  [1],
                  [Whether FD_CLOEXEC is defined.])
                  ])

GDA_BUILDDATE=`date -u -r ChangeLog '+%F'`
AC_SUBST(GDA_BUILDDATE, $GDA_BUILDDATE)

dnl **********************
dnl Cross compilation test
dnl **********************
AM_CONDITIONAL(CROSS_COMPILING, test $cross_compiling = yes)
AC_MSG_CHECKING([for a C compiler for build tools])
if test $cross_compiling = yes; then
  AC_CHECK_PROGS(CC_FOR_BUILD, gcc cc)
  CC_FOR_BUILD="CPATH=\"\" $CC_FOR_BUILD"
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
      if (./conftest) 2>&AS_MESSAGE_LOG_FD; then
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
AC_SEARCH_LIBS([strerror],[cposix])
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AM_PROG_AR
AC_HEADER_STDC
LT_PREREQ([2.2.6])
LT_INIT([disable-static])
AC_PROG_MAKE_SET

dnl -- Initialize help system
AC_ARG_WITH(help,
AS_HELP_STRING([--with-help], [Enable building help (GdaBrowser)]),
	[with_gdu=$withval]
	,with_gdu=yes)
if test x$with_gdu != xno; then
   YELP_HELP_INIT
   AC_DEFINE(HAVE_GDU, [1], [Help (GdaBrowser documentation) is built])
fi
AM_CONDITIONAL(HAVE_GDU, test x$with_gdu != xno)

dnl ******************************
dnl glib utilities
dnl ******************************
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
if test -z "$GLIB_GENMARSHAL"; then
   AC_MSG_ERROR([glib-genmarshal not found])
fi
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)
if test -z "$GLIB_MKENUMS"; then
   AC_MSG_ERROR([glib-mkenums not found])
fi
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)
if test -z "$GLIB_COMPILE_RESOURCES"; then
   AC_MSG_ERROR([glib-compile-resources not found])
fi

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

PKG_CHECK_MODULES(COREDEPS, [
	glib-2.0	>= req_ver_glib
	gio-2.0		>= req_ver_glib
	gobject-2.0	>= req_ver_glib
	gthread-2.0	>= req_ver_glib
	gmodule-no-export-2.0	>= req_ver_glib
	libxml-2.0
])
TST_CFLAGS="$COREDEPS_CFLAGS"
COREDEPS_CFLAGS="$COREDEPS_CFLAGS -DLIBGDA_ABI_NAME=$LIBGDA_ABI_NAME"


dnl ******************************
dnl Checks for UI extension
dnl ******************************
have_ui=no
have_goocanvas=no
have_graphviz=no
have_sourceview=no
have_glade=no
AC_ARG_WITH(ui,
	AS_HELP_STRING([--with-ui], [Enable GTK+ extension and tools]),
	,with_ui=auto)

if test "$with_ui" = "auto" -o "$with_ui" = "yes"
then
	GTK_MODULES="gtk+-3.0 >= 3.12.0"
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
if test "x$GTK_CFLAGS" != "x"
then
	GTK_CFLAGS="$GTK_CFLAGS -DHAVE_GTK_CLASSES"
fi
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
	PKG_CHECK_MODULES(GDKPIXBUF, "gdk-pixbuf-2.0", [
			AC_DEFINE(HAVE_GDKPIXBUF, [1], [Gdkpixbuf support enabled])
			have_gdkpixbuf=yes], [have_gdkpixbuf=no])

	if test "$with_sourceview" = "auto" -o "$with_sourceview" = "yes"
	then
		PKG_CHECK_MODULES(GTKSOURCEVIEW, "gtksourceview-3.0", [
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
		PKG_CHECK_MODULES(GOOCANVAS, "goocanvas-2.0", [
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
		if test "x$have_graphviz" = "xyes"
		then
			dnl test if new API is supported
			AC_MSG_CHECKING([whether Graphviz's new API is supported])
			graphviz_save_CFLAGS=$CFLAGS
			CFLAGS="$CFLAGS $GRAPHVIZ_CFLAGS"
			AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <gvc.h>
int main() {
    Agraph_t *graph;
    graph = agopen ("BrowserCanvasLayout", Agdirected, NULL);
    return 0;
}
])],
                        graphviz_new_api=yes, graphviz_new_api=no)
			CLFAGS=$graphviz_save_CFLAGS

			AC_MSG_RESULT($graphviz_new_api)
			if test "$graphviz_new_api" = "yes"; then
			   	AC_DEFINE(GRAPHVIZ_NEW_API,[1],[define if Graphviz's new API is available])
			fi
		fi
	fi

	PKG_CHECK_MODULES(GLADE, "gladeui-2.0", [
			AC_DEFINE(HAVE_GLADE, [1], [Install Glade Catalog])
			have_glade=yes
                        if test "x$GLADE_CATALOG_SEARCH_PATH" = "x"; then
                                GLADE_CATALOG=`$PKG_CONFIG --variable=catalogdir gladeui-2.0`
                        else
                                GLADE_CATALOG=$GLADE_CATALOG_SEARCH_PATH
                        fi
                        if test "x$GLADE_PIXMAP_DIR" = "x"; then
			        GLADE_PIXMAP=`$PKG_CONFIG --variable=pixmapdir gladeui-2.0`
                        else
			        GLADE_PIXMAP=$GLADE_PIXMAP_DIR
                        fi
                        ], [have_glade=no])
fi

AM_CONDITIONAL(HAVE_GDKPIXBUF, test x"$have_gdkpixbuf" = "xyes")
AC_SUBST(GDKPIXBUF_CFLAGS)
AC_SUBST(GDKPIXBUF_LIBS)

AM_CONDITIONAL(HAVE_GTKSOURCEVIEW, test x"$have_sourceview" = "xyes")
AC_SUBST(GTKSOURCEVIEW_CFLAGS)
AC_SUBST(GTKSOURCEVIEW_LIBS)

AM_CONDITIONAL(HAVE_GOOCANVAS, test x"$have_goocanvas" = "xyes")
AC_SUBST(GOOCANVAS_CFLAGS)
AC_SUBST(GOOCANVAS_LIBS)


AM_CONDITIONAL(HAVE_GRAPHVIZ, test x"$have_graphviz" = "xyes")
AC_SUBST(GRAPHVIZ_CFLAGS)
AC_SUBST(GRAPHVIZ_LIBS)

AM_CONDITIONAL(HAVE_GLADE, test x"$have_glade" = "xyes")
AC_SUBST(GLADE_CFLAGS)
AC_SUBST(GLADE_LIBS)
AC_SUBST(GLADE_CATALOG)
AC_SUBST(GLADE_PIXMAP)


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
EXPORT_PROV_SYM_REGEX='-export-symbols-regex "^(plugin_|Java_|g_module|gdaprov_).*"'
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
    COREDEPS_LIBS="$COREDEPS_LIBS -lwsock32 -lws2_32"
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
    COREDEPS_LIBS="$COREDEPS_LIBS -framework Carbon"
  ;;
*)
    AC_MSG_RESULT([Unix])
    NO_UNDEFINED=''
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

dnl ******************************
dnl Option to avoid building tools
dnl See bug #684895
dnl ******************************
AC_ARG_ENABLE([tools],
	AS_HELP_STRING([--disable-tools], [Disable building and installing Libgda's associated tools (gda-sql, control-center, browser, ...)]),
	[enable_tools=$enableval],[enable_tools=yes])

AM_CONDITIONAL(ENABLE_TOOLS, [test "$enable_tools" = "yes"])

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
	[enable_json=$enableval],[enable_json=no])

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
GOBJECT_INTROSPECTION_CHECK([1.30.0])
AC_ARG_ENABLE([gda-gi],
    AS_HELP_STRING([--enable-gda-gi[=@<:@no/auto/yes@:>@]], [Enable GObject Introspection for libgda [default=no]]),
    [enable_gda_gi=$enableval],[enable_gda_gi=auto])

if test "x$enable_introspection" != "xno" -a "x$found_introspection" = "xyes"
then
        enable_gda_gi="yes"
else
        enable_gda_gi="no"
fi

if test "x$enable_gda_gi" = "xyes" -a "x$found_introspection" != "xyes"
then
	AC_MSG_ERROR([Introspection for Libgda is requested but not available])
fi

AM_CONDITIONAL(ENABLE_GDA_GI, [test "$enable_gda_gi" = "yes"])

AC_ARG_ENABLE([gdaui-gi],
    AS_HELP_STRING([--enable-gdaui-gi[=@<:@nauto/yes/no@:>@]], [Enable GObject Introspection for libgda-ui [default=yes]]),
    [enable_gdaui_gi=$enableval],[enable_gdaui_gi=auto])

if test "x$enable_gdaui_gi" != "xno" -a "x$enable_gda_gi" = "xyes"
then
	enable_gdaui_gi="yes"
else
	enable_gdaui_gi="no"
fi

if test "x$enable_gdaui_gi" = "xyes" -a "x$have_ui" != "xyes"
then
	AC_MSG_ERROR([Introspection for Libgda UI is requested but no GTK+ is available])
fi

AM_CONDITIONAL(ENABLE_GDAUI_GI, [test "$enable_gdaui_gi" = "yes"])

AC_ARG_ENABLE([gi-system-install],
    AS_HELP_STRING([--enable-gi-system-install[=@<:@no/auto/yes@:>@]], [Install GObject Introspection files along with system installed files [default=yes]]),
    [enable_gi_system_install=$enableval],[enable_gi_system_install=yes])
AM_CONDITIONAL(ENABLE_GI_SYSTEM_INSTALL, [test x"$enable_gi_system_install" = "xyes"])

if test "x$enable_gda_gi" != "xyes" -a "x$enable_gdaui_gi" = "xyes"
then
	AC_MSG_ERROR([GObject Introspection for GDA-UI is requested but GObject Introspection for GDA is disabled. Use --enable-gda-gi])
fi

dnl ******************************
dnl Check for Vala Compiler
dnl ******************************
AM_PROG_VALAC()
enable_vala="no"
if test "x$VALAC" != "xno"; then
    enable_vala="yes"
fi
AC_PATH_PROG([VAPIGEN],[vapigen],[no])
AC_SUBST([VAPIGEN])
enable_vapigen="no"
if test "x$VAPIGEN" != "xno"; then
    enable_vapigen="yes"
fi

AC_ARG_ENABLE([gda-vala],
    AS_HELP_STRING([--enable-gda-vala[=@<:@auto/yes/no@:>@]], [Enable Vala bindings generation [default=auto]]),
    [enable_vala_bindings=$enableval],[enable_vala_bindings=auto])

if test "x$enable_vapigen" = "xyes" -a "x$enable_vala_bindings" != "xno" -a "x$found_introspection" = "xyes"
then
	enable_vala_bindings="yes"
fi
AM_CONDITIONAL(ENABLE_VALA_BINDINGS, test "x$enable_vala_bindings" = "xyes")

enable_gdaui_vala="auto"
AC_ARG_ENABLE([gdaui-vala],
    AS_HELP_STRING([--enable-gdaui-vala[=@<:@auto/yes/no@:>@]], [Enable Vala bindings for GDA-UI [default=auto]]),
    [enable_gdaui_vala=$enableval],[enable_gdaui_vala=auto])
if test "x$enable_vapigen" = "xyes" -a "x$enable_gdaui_vala" != "xno" -a "x$enable_vala_bindings" = "xyes"
then
	enable_gdaui_vala="yes"
fi
AM_CONDITIONAL(ENABLE_GDAUI_VALA, test "x$enable_gdaui_vala" = "xyes")

dnl ******************************
dnl Checks for libcrypto
dnl ******************************
AC_ARG_ENABLE([crypto],
	AS_HELP_STRING([--disable-crypto], [Disable SQLCipher database provider (using the libcrypto library, part of OpenSSL)]),
	[enable_crypto=$enableval],[enable_crypto=yes])

if test "x$enable_crypto" = xyes
then
        LIBCRYPTO_MODULES="libcrypto"
	PKG_CHECK_MODULES(LIBCRYPTO, $LIBCRYPTO_MODULES,
	        if test "$enable_crypto" = "yes"
	        then
		        AC_DEFINE(HAVE_LIBCRYPTO, [], [Defined if cryptographic support is enabled])
	        fi,
	        if test "$enable_crypto" = "yes"
	        then
			AC_MSG_ERROR([libcrypto (part of OpenSSL) support requested but not found])
	        fi)
fi

AM_CONDITIONAL(HAVE_LIBCRYPTO, [test "$enable_crypto" = "yes"])
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
			AM_CONDITIONAL(HAVE_LIBGCRYPT, false)
			echo "Password entry plugin will not be compiled, install libgcrypt >= 1.1.42 and reconfigure to make available."
      		else
			LIBGCRYPT_CFLAGS=`$LIBGCRYPT_CONFIG $libgcrypt_config_args --cflags`
        		LIBGCRYPT_LIBS=`$LIBGCRYPT_CONFIG $libgcrypt_config_args --libs`
			AM_CONDITIONAL(HAVE_LIBGCRYPT, true)
        	fi
	else
	LIBGCRYPT_CFLAGS=""
      	LIBGCRYPT_LIBS=""
	AM_CONDITIONAL(HAVE_LIBGCRYPT, false)
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

GTK_DOC_CHECK([1.14],[--flavour no-tmpl])

dnl ******************************
dnl Vala documentation
dnl ******************************
AC_PATH_PROG(VALADOC,[valadoc],[no])
enable_vala_doc_ext="no"
if test "x$VALADOC" != "xno" -a "x$enable_vala_ext" = "xyes"
then
    enable_vala_doc_ext="yes"
fi
AM_CONDITIONAL(ENABLE_VALA_DOC_EXT, [test "$VALADOC" != "no"])
enable_vala_doc="no"
if test "x$enable_gtk_doc" = "xyes"
then
dnl Find yelp-build program
    AC_PATH_PROG(YELP_BUILD, [yelp-build], [no])
    if test "x$YELP_BUILD" = "xno";
    then
        AC_MSG_RESULT([Vala documentation not build because yelp-build has not been found])
    else
        AC_SUBST(YELP_BUILD)
        enable_vala_doc="yes"
    fi
fi
AM_CONDITIONAL(ENABLE_VALA_DOC, [test "$enable_vala_doc" = "yes"])

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
POSTGRES_CHECK($lib)

dnl test for ORACLE
ORACLE_CHECK($lib)

dnl Test for JAVA and JDBC
JAVA_CHECK()

dnl Test for LDAP
LDAP_CHECK($lib)

dnl test for FireBird
FIREBIRD_CHECK($lib)

dnl Test for MDB Tools (for MS Access files)
MDBTOOLS_CHECK([$lib])

dnl test for system-installed SQLite
dnl If no SQLite is found or if the SQLite found does not have the sqlite3_table_column_metadata()
dnl function call, then use the embedded version
dnl also, on Win32, always use the embedded version
if test x"$platform_win32" = "xyes"
then
        AM_CONDITIONAL(HAVE_SQLITE, false)
else
	AC_ARG_ENABLE(system-sqlite,
		AS_HELP_STRING([--enable-system-sqlite], [Use SQLite installed on the system [default=no]]),
        	[use_syst_sqlite=$enableval], [use_syst_sqlite="no"])

	if test "x$use_syst_sqlite" = "xyes"; then
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
                		AC_MSG_RESULT([Note: using system installed version of SQLite, meta data for functions will not be available])
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


dnl ************************************
dnl Check for libsecret or gnome-keyring
dnl ************************************

AC_ARG_WITH(libsecret,
	AS_HELP_STRING([--with-libsecret], [Enable libsecret support]),
	,with_libsecret=auto)

AC_ARG_WITH(gnome-keyring,
	AS_HELP_STRING([--with-gnome-keyring], [Enable gnome-keyring support]),
	,with_gnome_keyring=auto)

if test "$with_libsecret" = "auto" -o "$with_libsecret" = "yes"
then
	PKG_CHECK_MODULES(LIBSECRET, libsecret-1, [
		AC_DEFINE(HAVE_LIBSECRET, [1], [libsecret support enabled])
		have_libsecret=yes], [
		if test "$with_libsecret" = "yes"
		then
			AC_MSG_ERROR([libsecret support requested but not found.])
		fi
		have_libsecret=no])
fi
AM_CONDITIONAL(LIBSECRET, test "$have_libsecret" = "yes")
AC_SUBST(LIBSECRET_CFLAGS)
AC_SUBST(LIBSECRET_LIBS)

if test "$have_libsecret" = "no"
then
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

dnl *****************
dnl Check for eventfd
dnl *****************
AC_ARG_ENABLE([eventfd], [AS_HELP_STRING([--disable-eventfd], [disable eventfd [default=no]])],
    [disable_eventfd=yes], [disable_eventfd=no])

if test "x$disable_eventfd" != "xyes"; then
    AC_CHECK_HEADERS(sys/eventfd.h,
                     [AC_DEFINE(HAVE_EVENTFD, 1, [Have eventfd extension.])])
fi

dnl
dnl TERMIOS.H
dnl
AC_CHECK_HEADERS([termios.h])

dnl
dnl where to install DTD files
dnl
LIBGDA_DTDDIR=${datadir}/libgda-$LIBGDA_ABI_VERSION/dtd
AC_SUBST(LIBGDA_DTDDIR)

dnl
dnl Check if lib should be build with the debug mode
dnl
gda_debug_flags=""
AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug], [Enable debug mode [default=no]]),
        [use_debug=$enableval], [use_debug="no"])

if test "x$use_debug" = "xyes"
then
	AC_DEFINE(GDA_DEBUG)
	GDA_DEBUG_FLAGS=-DGDA_DEBUG
	AC_SUBST(GDA_DEBUG_FLAGS)
fi

AC_ARG_ENABLE(mutex-debug,
	AS_HELP_STRING([--enable-mutex-debug], [Enable debug mode for GdaMutex [default=no]]),
        [use_debug=$enableval], [use_debug="no"])

if test "x$use_debug" = "xyes"
then
	AC_DEFINE(GDA_DEBUG_MUTEX)
	GDA_DEBUG_FLAGS="$GDA_DEBUG_FLAGS -DGDA_DEBUG_MUTEX"
	GDA_DEBUG_LDFLAGS=-rdynamic
	AC_SUBST(GDA_DEBUG_FLAGS)
	AC_SUBST(GDA_DEBUG_LDFLAGS)
fi

dnl Disable rebuild of glib-mkenum -generated source code:
AC_ARG_ENABLE(rebuilds,
	AS_HELP_STRING([--disable-rebuilds], [disable all source autogeneration rules]),
	[enable_rebuilds=$enableval], [enable_rebuilds=yes])

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
AC_ARG_ENABLE(default-binary,
	AS_HELP_STRING([--enable-default-binary], [install binaries as default (default=gda_stable)]),
	[enable_default_binary=$enableval], [enable_default_binary=gda_stable])

AM_CONDITIONAL(DEFAULT_BINARY, test "x$enable_default_binary" = xyes)

#
# Control if Examples should be built.
#
AC_ARG_ENABLE([examples],
	AS_HELP_STRING([--enable-examples], [Enable examples compilation [default=no]]),
	[enable_examples=$enableval],
	[enable_examples="no"])

AM_CONDITIONAL(ENABLE_EXAMPLES, [test "$enable_examples" = "yes"])
# We use -Wno-address because SQLITE3_CALL() has an ifdefed definition that 
#   makes it hard to always avoid the warning.
# We use -Wno-unused-variable because the lemon parser generator (in libgda's sources),
#   creates some generic macros that declare variables that are not always used. 
# We use -Wno-unused-parameter because the lemon parser generator (in libgda's sources),
#   produces generic code, and it's hard to fix that generated code.
# We use -Wno-pointer-sign because the sqlite source code (copied into libgda's sources)
#   has problems but we do not want to patch that code.
# We use -Wno-missing-field-initializers because recent glib versions introduced this 
#  and they refuse to fix it: https://bugzilla.gnome.org/show_bug.cgi?id=662797
#  We should remove -Wno-missing-field-initializers when we replace the use of 
#  the deprecated G_STATIC_MUTEX_INIT macro.
DK_ARG_ENABLE_WARNINGS([COREDEPS_WFLAGS],
	[-Wall],
	[-Wall -Wextra -Wformat-security -Wno-address -Wno-unused-variable -Wno-unused-parameter -Wno-pointer-sign -Wno-missing-field-initializers -DGSEAL_ENABLE],
	[PANGO G GDK GDK_PIXBUF GTK])

AC_SUBST(COREDEPS_CFLAGS)
AC_SUBST(COREDEPS_LIBS)

AC_CONFIG_FILES([
Makefile
libgda.spec
libgda-6.0.pc
libgda-ui-6.0.pc
libgda-report-6.0.pc
libgda-xslt-6.0.pc
po/Makefile.in
libgda/Makefile
libgda/libgda.h
libgda/binreloc/Makefile
libgda/handlers/Makefile
libgda/providers-support/Makefile
libgda/sql-parser/Makefile
libgda/sqlite/Makefile
libgda/sqlite/sqlite-src/Makefile
libgda/sqlite/virtual/libgda-virtual.h
libgda/sqlite/virtual/Makefile
libgda/thread-wrapper/Makefile
providers/Makefile
providers/reuseable/Makefile
providers/reuseable/postgres/Makefile
providers/reuseable/mysql/Makefile
providers/bdb/Makefile
providers/bdb/libgda-bdb-6.0.pc
providers/bdbsql/Makefile
providers/bdbsql/libgda-bdbsql-6.0.pc
providers/mdb/Makefile
providers/mdb/libmdb-src/Makefile
providers/mdb/libgda-mdb-6.0.pc
providers/mysql/Makefile
providers/mysql/libgda-mysql-6.0.pc
providers/oracle/Makefile
providers/oracle/libgda-oracle-6.0.pc
providers/postgres/Makefile
providers/postgres/libgda-postgres-6.0.pc
providers/firebird/Makefile
providers/firebird/libgda-firebird-6.0.pc
providers/sqlite/Makefile
providers/sqlite/libgda-sqlite-6.0.pc
providers/jdbc/Makefile
providers/jdbc/libgda-jdbc-6.0.pc
providers/ldap/Makefile
providers/ldap/libgda-ldap-6.0.pc
providers/web/Makefile
providers/web/libgda-web-6.0.pc
providers/skel-implementation/Makefile
providers/skel-implementation/capi/Makefile
providers/skel-implementation/capi/libgda-capi-6.0.pc
providers/skel-implementation/models/Makefile
providers/skel-implementation/models/libgda-models-6.0.pc
providers/sqlcipher/Makefile
providers/sqlcipher/libgda-sqlcipher-6.0.pc
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
libgda-ui/glade/Makefile
control-center/Makefile
control-center/gda-control-center-6.0.desktop.in
control-center/data/Makefile
tools/Makefile
tools/gda-sql-6.0.1:tools/gda-sql.1.in
tools/base/Makefile
tools/browser/Makefile
tools/browser/gda-browser-$API_VERSION.desktop.in:tools/browser/gda-browser.desktop.in.in
tools/browser/canvas/Makefile
tools/browser/dummy-perspective/Makefile
tools/browser/schema-browser/Makefile
tools/browser/query-exec/Makefile
tools/browser/data-manager/Makefile
tools/browser/ldap-browser/Makefile
tools/browser/help/Makefile
tools/common/Makefile
tools/misc/Makefile
tools/help/Makefile
tools/help/C/Makefile
testing/Makefile
tests/Makefile
tests/parser/Makefile
tests/data-models/Makefile
tests/providers/Makefile
tests/value-holders/Makefile
tests/meta-store/Makefile
tests/multi-threading/Makefile
tests/ui/Makefile
tests/ddl/Makefile
doc/Makefile
doc/C/Makefile
doc/C/libgda.types
doc/C/version.xml
doc/C/builddate.xml
doc/mallard/Makefile
data/Makefile
examples/Makefile
examples/DDL/Makefile
stamp.h
])
AC_OUTPUT


dnl Dirty work to be able to build documentation
chmod -R u+w ${srcdir}/doc/C
cp doc/C/version.xml ${srcdir}/doc/C/version.xml.tmp
mv ${srcdir}/doc/C/version.xml.tmp ${srcdir}/doc/C/version.xml
cp doc/C/builddate.xml ${srcdir}/doc/C/builddate.xml.tmp
mv ${srcdir}/doc/C/builddate.xml.tmp ${srcdir}/doc/C/builddate.xml

cp ${srcdir}/doc/C/libgda-sections.txt doc/C/libgda-6.0-sections.txt
echo "" >> doc/C/libgda-6.0-sections.txt
cat ${srcdir}/doc/C/libgda-ui-sections.txt >> doc/C/libgda-6.0-sections.txt
cp doc/C/libgda-6.0-sections.txt ${srcdir}/doc/C/libgda-6.0-sections.txt.tmp
mv ${srcdir}/doc/C/libgda-6.0-sections.txt.tmp ${srcdir}/doc/C/libgda-6.0-sections.txt

cp doc/C/libgda.types doc/C/libgda-6.0.types
if test x$have_ui != xno
then
	echo "" >> doc/C/libgda-6.0.types
	cat ${srcdir}/doc/C/libgda-ui.types >> doc/C/libgda-6.0.types
fi


dnl Print configuration summary
echo ""
echo " Configuration summary for version $GDA_VERSION"
echo "   Installation prefix = $prefix"
echo "   Building GTK+ UI extension: `if test x$have_ui != xno; then echo yes; else echo no; fi`"
echo "   Installing Glade GTK+ UI extension catalog: `if test x$have_glade != xno; then echo yes; else echo no; fi`"
echo "   Building Libxslt extension: `if test x$have_xslt != xno; then echo yes; else echo no; fi`"
echo "   Building libgda GObject Introspection: `if test x$enable_gda_gi = xyes; then echo yes; else echo no; fi`"
echo "   Building libgda-ui GObject Introspection: `if test x$enable_gdaui_gi != xno; then echo yes; else echo no; fi`"
echo "   Building Gtk-Doc: `if test x$enable_gtk_doc != xno; then echo yes; else echo no; fi`"
echo "   Building Examples: `if test x$enable_examples = xyes; then echo yes; else echo no; fi`"
echo "   Building Help (GdaBrowser): `if test x$with_gdu != xno; then echo yes; else echo no; fi`"
echo "   Building GDA Vala Bindings: `if test x$enable_vala_bindings != xyes; then echo no; else echo yes; fi`"
echo "   Building GDA-UI Vala Bindings: `if test "x$enable_gdaui_vala" = "xyes"; then echo yes; else echo no; fi`"
echo "   Building Vala Documentation:"
echo "      Building GDA Vala Docs: `if test x$enable_vala_doc != xno; then echo yes; else echo no; fi`"
echo "   Building Building Libgda's associated tools: `if test x$enable_tools = xyes; then echo yes; else echo no; fi`"
echo "   Compiled providers:"
echo "      Berkeley DB = $bdb_found"
echo "      Berkeley DB SQL = $bdbsql_found"
echo "      FireBird (client)= $firebird_client_found"
echo "      FireBird (embed)= $firebird_embed_found"
echo "      MDB (MS Access) = $mdbtools_found"
echo "      MySQL = $mysql_found"
echo "      Oracle = $oracle_found"
echo "      PostgreSQL = $postgres_found"
echo "      SQLite = yes `if test x$have_sqlite = xyes; then echo '(from system installation)'; else echo '(embedded)'; fi`"
echo "      SQLCipher = `if test x$enable_crypto != xyes; then echo no; else echo yes; fi`"
echo "      JDBC = $java_found"
echo "      WEB = `if test x$have_libsoup = xyes; then echo yes; else echo no; fi`"
echo "      LDAP = `if test x$ldap_found = xyes; then echo yes; else echo no; fi`"
if test x"$br_cv_binreloc" != "xyes" -a x"$platform_win32" != "xyes"
then
       echo "   Binreloc support is disabled: Libgda will not be relocatable. To enable binreloc support re-run with --enable-binreloc (see http://autopackage.org/docs/binreloc for more information)"
fi
echo ""
