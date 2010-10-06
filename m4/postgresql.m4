dnl -*- mode: autoconf -*-
dnl Copyright 2010 Vivien Malerba
dnl
dnl SYNOPSIS
dnl
dnl   POSTGRES_CHECK([libdirname])
dnl
dnl   [libdirname]: defaults to "lib". Can be overridden by the --with-postgres-libdir-name option
dnl
dnl DESCRIPTION
dnl
dnl   This macro tries to find the PostgreSQL libraries and header files
dnl
dnl   It defines two options:
dnl   --with-postgres=yes/no/<directory>
dnl   --with-postgres-libdir-name=<dir. name>
dnl
dnl   If the 1st option is "yes" then the macro tries to use pg_config to locate
dnl   the PostgreSQL package, and if it fails, it tries in several well known directories
dnl
dnl   If the 1st option is "no" then the macro does not attempt at locating the
dnl   postgresql package
dnl
dnl   If the 1st option is a directory name, then the macro tries to locate the postgresql package
dnl   in the specified directory.
dnl
dnl   If the macro has to try to locate the postgresql package in one or more directories, it will
dnl   try to locate the header files in $dir/include and the library files in $dir/lib, unless
dnl   the second option is used to specify a directory name to be used instead of "lib" (for
dnl   example lib64).
dnl
dnl USED VARIABLES
dnl
dnl   $linklibext: contains the library suffix (like ".so"). If not specified ".so" is used.
dnl   $platform_win32: contains "yes" on Windows platforms. If not specified, assumes "no"
dnl
dnl
dnl DEFINED VARIABLES
dnl
dnl   This macro always calls:
dnl
dnl    AC_SUBST(POSTGRES_LIBS)
dnl    AC_SUBST(POSTGRES_CFLAGS)
dnl    postgres_found=yes/no
dnl
dnl   and if the postgresql package is found:
dnl
dnl    AM_CONDITIONAL(POSTGRES, true)
dnl
dnl
dnl LICENSE
dnl
dnl This file is free software; the author(s) gives unlimited
dnl permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.
dnl

m4_define([_POSTGRES_CHECK_INTERNAL],
[
    AC_BEFORE([AC_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([AM_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([LT_INIT],[$0])dnl setup libtool first

    pg_loclibdir=$1
    if test "x$pg_loclibdir" = x
    then
        if test "x$platform_win32" = xyes
	then
	    pg_loclibdir=bin
	else
	    pg_loclibdir=lib
	fi
    fi

    # determine if PostgreSQL should be searched for
    # and use pg_config if the "yes" option is used
    postgres_found=no
    try_postgres=true
    pkgpostgres=no
    POSTGRES_LIBS=""
    postgres_test_dir=""
    AC_ARG_WITH(postgres,
              AS_HELP_STRING([--with-postgres[=@<:@yes/no/<directory>@:>@]],
                             [Locate PostgreSQL files]),[
			     if test $withval = no
			     then
			         try_postgres=false
			     elif test $withval != yes
			     then
			         postgres_test_dir=$withval
			     fi])
    AC_ARG_WITH(postgres-libdir-name,
              AS_HELP_STRING([--with-postgres-libdir-name[=@<:@<dir. name>@:>@]],
                             [Locate PostgreSQL library file, related to the PostgreSQL prefix specified from --with-postgres]),
			     [pg_loclibdir=$withval])

    # try with the default available pg_config
    if test $try_postgres = true -a "x$postgres_test_dir" = x
    then
        AC_PATH_PROGS(PG_CONFIG, pg_config)
	if test "x$PG_CONFIG" != x
	then
	    pkgpostgres=yes
	    POSTGRES_CFLAGS="-I`$PG_CONFIG --includedir`"
	    POSTGRES_LIBS="-L`$PG_CONFIG --libdir` -lpq"
	else
	    postgres_test_dir="/usr /usr/local /opt/gnome"
	fi
    fi

    # try to locate pg_config in places in $postgres_test_dir
    if test $try_postgres = true
    then
	if test $pkgpostgres = no
	then
	    if test "x$linklibext" = x
	    then
	        postgres_libext=".so"
	    else
	        postgres_libext=".dll"
	    fi
	    if test $platform_win32 = yes
	    then
	        for d in $postgres_test_dir
	        do
	            AC_MSG_CHECKING([checking for PostgreSQL files in $d])
		    echo "looking for $d/include/libpq-fe.h -a -f $d/$pg_loclibdir/libpq$postgres_libext"
		    if test -a $d/include/libpq-fe.h -a -f $d/$pg_loclibdir/libpq$postgres_libext
		    then
			save_CFLAGS="$CFLAGS"
	                CFLAGS="$CFLAGS -I$d/include"
  	                save_LIBS="$LIBS"
	                LIBS="$LIBS -L$d/$pg_loclibdir -lpq"
   	                AC_LINK_IFELSE([[
#include <libpq-fe.h>
int main() {
    printf("%p", PQconnectdb);
    return 0;
}
]],
	                             postgres_found=yes)
	                CFLAGS="$save_CFLAGS"
  	                LIBS="$save_LIBS"
			if test "x$postgres_found" = xyes
			then
		            AC_MSG_RESULT([found])
			    POSTGRES_CFLAGS=-I$d/include
			    POSTGRES_LIBS="-L$d/$pg_loclibdir -lpq"
			    break
			fi
			AC_MSG_RESULT([files found but are not useable])
		    else
		        AC_MSG_RESULT([not found])
		    fi
	        done
	    else
	        for d in $postgres_test_dir
	        do
	            AC_MSG_NOTICE([checking for pg_config tool in $d])
                    AC_PATH_PROGS(PG_CONFIG, pg_config,,[$d/bin])
		    if test "x$PG_CONFIG" != x
		    then
	    	        pkgpostgres=yes
	    	        POSTGRES_CFLAGS="-I`$PG_CONFIG --includedir`"
	    	        POSTGRES_LIBS="-L`$PG_CONFIG --libdir` -lpq"
	    	        break;
		    fi
	        done
	    fi
        fi
	if test "x$POSTGRES_LIBS" = x
	then
	    AC_MSG_NOTICE([POSTGRESQL backend not used])
	else
    	    postgres_found=yes
	fi
    fi

    AM_CONDITIONAL(POSTGRES,[test "$postgres_found" = "yes"])
    AC_SUBST(POSTGRES_LIBS)
    AC_SUBST(POSTGRES_CFLAGS)
])


dnl Usage:
dnl   POSTGRES_CHECK([libdirname])

AC_DEFUN([POSTGRES_CHECK],
[
    _POSTGRES_CHECK_INTERNAL([$1])
])
