dnl -*- mode: autoconf -*-
dnl Copyright 2010 Vivien Malerba
dnl
dnl SYNOPSIS
dnl
dnl   MYSQL_CHECK([libdirname])
dnl
dnl   [libdirname]: defaults to "lib". Can be overridden by the --with-mysql-libdir-name option
dnl
dnl DESCRIPTION
dnl
dnl   This macro tries to find the Mysql libraries and header files
dnl
dnl   It defines two options:
dnl   --with-mysql=yes/no/<directory>
dnl   --with-mysql-libdir-name=<dir. name>
dnl
dnl   If the 1st option is "yes" then the macro tries to use mysql-config to locate
dnl   the MySQL package, and if it fails, it tries in several well known directories
dnl
dnl   If the 1st option is "no" then the macro does not attempt at locating the
dnl   mysql package
dnl
dnl   If the 1st option is a directory name, then the macro tries to locate the mysql package
dnl   in the specified directory.
dnl
dnl   If the macro has to try to locate the mysql package in one or more directories, it will
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
dnl    AC_SUBST(MYSQL_LIBS)
dnl    AC_SUBST(MYSQL_CFLAGS)
dnl    mysql_found=yes/no
dnl
dnl   and if the mysql package is found:
dnl
dnl    AM_CONDITIONAL(MYSQL, true)
dnl
dnl
dnl LICENSE
dnl
dnl This file is free software; the author(s) gives unlimited
dnl permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.
dnl

m4_define([_MYSQL_CHECK_INTERNAL],
[
    AC_BEFORE([AC_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([AM_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([LT_INIT],[$0])dnl setup libtool first

    mysql_loclibdir=$1
    if test "x$mysql_loclibdir" = x
    then
        if test "x$platform_win32" = xyes
	then
	    mysql_loclibdir=lib/opt
	else
	    mysql_loclibdir=lib
	fi
    fi

    # determine if Mysql should be searched for
    # and use mysql_config if the "yes" option is used
    mysql_found=no
    try_mysql=true
    pkgmysql=no
    MYSQL_LIBS=""
    mysql_test_dir=""
    AC_ARG_WITH(mysql,
              AS_HELP_STRING([--with-mysql[=@<:@yes/no/<directory>@:>@]],
                             [Locate Mysql files]),[
			     if test $withval = no
			     then
			         try_mysql=false
			     elif test $withval != yes
			     then
			         mysql_test_dir=$withval
			     fi])
    AC_ARG_WITH(mysql-libdir-name,
              AS_HELP_STRING([--with-mysql-libdir-name[=@<:@<dir. name>@:>@]],
                             [Locate Mysql library file, related to the MYSQL prefix specified from --with-mysql]),
			     [mysql_loclibdir=$withval])

    # try with the default available mysql_config
    if test $try_mysql = true -a "x$mysql_test_dir" = x
    then
        AC_PATH_PROGS(MYSQL_CONFIG, mysql_config mysql_config5)
	if test "x$MYSQL_CONFIG" != x
	then
	    pkgmysql=yes
	    MYSQL_CFLAGS=`$MYSQL_CONFIG --cflags`
	    MYSQL_LIBS=`$MYSQL_CONFIG --libs`
	else
	    mysql_test_dir="/usr /usr/local /opt/gnome"
	fi
    fi

    # try to locate mysql_config in places in $mysql_test_dir
    if test $try_mysql = true
    then
	if test $pkgmysql = no
	then
	    if test "x$linklibext" = x
	    then
	        mysql_libext=".so"
	    else
	        mysql_libext=".lib"
	    fi
	    if test $platform_win32 = yes
	    then
	        for d in $mysql_test_dir
	        do
	            AC_MSG_CHECKING([checking for mysql files in $d])
		    if test -a $d/include/mysql.h -a -f $d/$mysql_loclibdir/libmysql$mysql_libext
		    then
			save_CFLAGS="$CFLAGS"
	                CFLAGS="$CFLAGS -I$d/include"
  	                save_LIBS="$LIBS"
	                LIBS="$LIBS -L$d/$mysql_loclibdir -lmysql"
   	                AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <winsock.h>
#include <mysql.h>
int main() {
    printf("%p", mysql_real_connect);
    return 0;
}
])],
	                             mysql_found=yes)
	                CFLAGS="$save_CFLAGS"
  	                LIBS="$save_LIBS"
			if test "x$mysql_found" = xyes
			then
		            AC_MSG_RESULT([found])
			    MYSQL_CFLAGS=-I$d/include
			    MYSQL_LIBS="-L$d/$mysql_loclibdir -lmysql"
			    break
			fi
			AC_MSG_RESULT([files found but are not useable])
		    else
		        AC_MSG_RESULT([not found])
		    fi
	        done
	    else
	        for d in $mysql_test_dir
	        do
	            AC_MSG_NOTICE([checking for mysql_config tool in $d])
                    AC_PATH_PROGS(MYSQL_CONFIG, mysql_config mysql_config5,,[$d/bin])
		    if test "x$MYSQL_CONFIG" != x
		    then
	    	        pkgmysql=yes
	    	        MYSQL_CFLAGS=`$MYSQL_CONFIG --cflags`
	    	        MYSQL_LIBS=`$MYSQL_CONFIG --libs`
	    	        break;
		    fi
	        done
	    fi
	else
	    AC_MSG_CHECKING([checking for Mysql headers])
	    save_CFLAGS="$CFLAGS"
	    CFLAGS="$CFLAGS $MYSQL_CFLAGS"
            save_LIBS="$LIBS"
	    LIBS="$LIBS $MYSQL_LIBS"

            AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <mysql.h>
int main() {
    printf("%p", mysql_real_connect);
    return 0;
}
])],
	    mysql_found=yes)
	    CFLAGS="$save_CFLAGS"
            LIBS="$save_LIBS"
	    if test "x$mysql_found" = xyes
	    then
	        AC_MSG_RESULT([found])
	    else
	        MYSQL_LIBS=""
		MYSQL_CFLAGS=""
	        AC_MSG_RESULT([not found])
	    fi
        fi
	if test "x$MYSQL_LIBS" = x
	then
	    AC_MSG_NOTICE([MYSQL backend not used])
	else
    	    mysql_found=yes
	fi
    fi

    AM_CONDITIONAL(MYSQL,[test "$mysql_found" = "yes"])
    AC_SUBST(MYSQL_LIBS)
    AC_SUBST(MYSQL_CFLAGS)
])


dnl Usage:
dnl   MYSQL_CHECK([libdirname])

AC_DEFUN([MYSQL_CHECK],
[
    _MYSQL_CHECK_INTERNAL([$1])
])
