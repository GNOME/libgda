dnl -*- mode: autoconf -*-
dnl Copyright 2010 Vivien Malerba
dnl
dnl SYNOPSIS
dnl
dnl   MDBTOOLS_CHECK([libdirname],[GLib's CFLAGS],[GLib's LIBS])
dnl
dnl   [libdirname]: defaults to "lib". Can be overridden by the --with-mdb-libdir-name option
dnl   [GLib's CFLAGS]: default to `$PKG_CONFIG --cflags glib-2.0`
dnl   [GLib's LIBS]: default to `$PKG_CONFIG --libs glib-2.0`, used when pkg-config is not
dnl                  used to find the mdbtools package
dnl
dnl
dnl DESCRIPTION
dnl
dnl   This macro tries to find the MDBTools libraries and header files
dnl
dnl   It defines two options:
dnl   --with-mdb=yes/no/<directory>
dnl   --with-mdb-libdir-name=<dir. name>
dnl
dnl   If the 1st option is "yes" then the macro tries to use pkg-config to locate
dnl   the "libmdb" package, and if it fails, it tries in several well known directories
dnl
dnl   If the 1st option is "no" then the macro does not attempt at locating the
dnl   mdbtools package
dnl
dnl   If the 1st option is a directory name, then the macro tries to locate the mdbtools package
dnl   in the specified directory.
dnl
dnl   If the macro has to try to locate the mdbtools package in one or more directories, it will
dnl   try to locate the header files in $dir/include and the library files in $dir/lib, unless
dnl   the second option is used to specify a directory name to be used instead of "lib" (for
dnl   example lib64).
dnl
dnl USED VARIABLES
dnl
dnl   $linklibext: contains the library suffix (like ".so"). If not specified ".so" is used.
dnl
dnl
dnl DEFINED VARIABLES
dnl
dnl   This macro always calls:
dnl
dnl    AC_SUBST(MDB_LIBS)
dnl    AC_SUBST(MDB_CFLAGS)
dnl    mdbtools_found=yes (from where)/no
dnl
dnl   and if the mdbtools package is found:
dnl
dnl    AM_CONDITIONAL(MDB, true)
dnl    AC_DEFINE(MDB_WITH_WRITE_SUPPORT,[1],[define if mdb_open accepts MDB_WRITABLE])
dnl    AC_DEFINE(MDB_BIND_COLUMN_FOUR_ARGS,[1],[define if mdb_bind_column accepts four args])
dnl    AC_DEFINE(MDB_V07,[1],[define if Mdbtools is in version 0.7])
dnl
dnl
dnl LICENSE
dnl
dnl This file is free software; the author(s) gives unlimited
dnl permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.
dnl

m4_define([_MDBTOOLS_CHECK_INTERNAL],
[
    AC_BEFORE([AC_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([AM_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([LT_INIT],[$0])dnl setup libtool first

    mdb_loclibdir=$1
    if test "x$mdb_loclibdir" = x
    then
        if test "x$platform_win32" = xyes
	then
	    mdb_loclibdir=bin
	else
	    mdb_loclibdir=lib
	fi
    fi

    mdb_glib_cflags=$2
    if test "x$mdb_glib_cflags" = x
    then
	mdb_glib_cflags=`$PKG_CONFIG --cflags glib-2.0`
    fi

    mdb_glib_libs=$3
    if test "x$mdb_glib_libs" = x
    then
	mdb_glib_libs=`$PKG_CONFIG --libs glib-2.0`
    fi

    # determine if MDBTools should be searched for
    # and use pkg-config if the "yes" option is used
    mdbtools_found=no
    try_mdb=true
    pkgmdb=no
    MDB_LIBS=""
    mdb_test_dir=""
    AC_ARG_WITH(mdb,
              AS_HELP_STRING([--with-mdb[=@<:@yes/no/<directory>@:>@]],
                             [Locate MDBTools files for the MS Access backend (read only)]),[
			     if test $withval = no
			     then
			         try_mdb=false
			     elif test $withval != yes
			     then
			         mdb_test_dir=$withval
			     fi])
    AC_ARG_WITH(mdb-libdir-name,
              AS_HELP_STRING([--with-mdb-libdir-name[=@<:@<dir. name>@:>@]],
                             [Locate MDBTools library file, related to the MDB prefix specified from --with-mdb]),
			     [mdb_loclibdir=$withval])

    # try with pkgconfig
    if test $try_mdb = true -a "x$mdb_test_dir" = x
    then
	PKG_CHECK_MODULES(MDB, [libmdb >= 0.9],[pkgmdb=yes],[pkgmdb=no])
    fi

    # try to locate files if pkg-config did not already do its job
    if test $try_mdb = true
    then
	if test $pkgmdb = no
	then
	    if test "x$linklibext" = x
	    then
	        mdb_libext=".so"
	    else
	        mdb_libext="$linklibext"
	    fi
	    mdbdir=""
	    for d in $mdb_test_dir
	    do
	        AC_MSG_CHECKING([for MDB Tools files in $d])
	        if test -f $d/include/mdbtools.h -a -f $d/$mdb_loclibdir/libmdb$mdb_libext -o -f $d/include/mdbtools.h -a -f $d/$mdb_loclibdir/libmdb.a
	        then
  	            save_CFLAGS="$CFLAGS"
	            CFLAGS="$CFLAGS -I$d/include $mdb_glib_cflags"
  	            save_LIBS="$LIBS"
	            LIBS="$LIBS -L$d/$mdb_loclibdir -lmdb $mdb_glib_libs"
   	            AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <mdbtools.h>
int main() {
    printf("%p", mdb_open);
    return 0;
}
])],
	                         mdbdir=$d)
	            CFLAGS="$save_CFLAGS"
  	            LIBS="$save_LIBS"
	            if test x$mdbdir != x
		    then
		        AC_MSG_RESULT([found])
			MDB_CFLAGS=-I${mdbdir}/include
	    		MDB_LIBS="-L${mdbdir}/$mdb_loclibdir -lmdb"
		        break
  		    else
		        AC_MSG_RESULT([not found])
		    fi
	        else
	            AC_MSG_RESULT([not found])
	        fi
	    done
        fi

	if test "x$MDB_LIBS" = x
	then
	    AC_MSG_NOTICE([MDB backend not used])
	else
    	    mdbtools_found="yes (from system installation)"

  	    save_CFLAGS="$CFLAGS"
	    CFLAGS="$CFLAGS $MDB_CFLAGS $mdb_glib_cflags"

	    AC_MSG_CHECKING([whether mdb_open takes one or two arguments])
   	    AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <mdbtools.h>
int main() {
    const char *filename;
    mdb_open(filename, MDB_WRITABLE);
    return 0;
}
])],
	                     mdb_open_args=two, mdb_open_args=one)

	    AC_MSG_RESULT($mdb_open_args)
	    if test "$mdb_open_args" = "two"; then
		AC_DEFINE(MDB_WITH_WRITE_SUPPORT,[1],[define if mdb_open accepts MDB_WRITABLE])
	    fi

	    AC_MSG_CHECKING([whether mdb_bind_column takes three or four arguments])
	    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <mdbtools.h>
int main() {
	MdbHandle *mdb;
	int c;
	char *bound_data[256];
	int len;
	mdb_bind_column(mdb, c, bound_data[c], &len);
	return 0;
}
]])],
	                      mdb_bind_column_args=four, mdb_bind_column_args=three)

	    AC_MSG_RESULT($mdb_bind_column_args)
	    if test "$mdb_bind_column_args" = "four"; then
		AC_DEFINE(MDB_BIND_COLUMN_FOUR_ARGS,[1],[define if mdb_bind_column accepts four args])
	    fi

	    AC_MSG_CHECKING([whether Mdbtools is in version 0.7])
	    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <mdbtools.h>
int main() {
	int a;
	a = MDB_SDATETIME;
	return 0;
}
]])],
	                      mdb_v7=no, mdb_v7=yes)

	    AC_MSG_RESULT($mdb_v7)
	    if test "$mdb_v7" = "yes"; then
		AC_DEFINE(MDB_V07,[1],[define if Mdbtools is in version 0.7])
	    fi
	    CFLAGS="$save_CFLAGS"
	fi
    fi

    AM_CONDITIONAL(MDB,[test "$mdbtools_found" != "no"])
    AM_CONDITIONAL(MDBEMBEDDED, false)
    AC_SUBST(MDB_LIBS)
    AC_SUBST(MDB_CFLAGS)
])


dnl Usage:
dnl   MDBTOOLS_CHECK([libdirname],[GLib's CFLAGS],[GLib's LIBS])

AC_DEFUN([MDBTOOLS_CHECK],
[
     AC_ARG_ENABLE(system-mdbtools,
		AS_HELP_STRING([--enable-system-mdbtools], [Use MDBTools installed on the system [default=yes]]),
        	[use_syst_mdb=$enableval], [use_syst_mdb="yes"])
    if test "x$use_syst_mdb" = "xyes"; then
        _MDBTOOLS_CHECK_INTERNAL([$1],[$2],[$3])
    else
        AM_ICONV
        AM_CONDITIONAL(MDB, true)
        AM_CONDITIONAL(MDBEMBEDDED, true)
        AC_DEFINE(MDB_BIND_COLUMN_FOUR_ARGS,[1],[define if mdb_bind_column accepts four args])
        AC_DEFINE(MDB_WITH_WRITE_SUPPORT,[1],[define if mdb_open accepts MDB_WRITABLE])
        mdbtools_found="yes (embedded)"
    fi
])
