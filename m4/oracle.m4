dnl -*- mode: autoconf -*-
dnl Copyright 2010 Vivien Malerba
dnl
dnl SYNOPSIS
dnl
dnl   ORACLE_CHECK([libdirname])
dnl
dnl   [libdirname]: defaults to "lib". Can be overridden by the --with-oracle-libdir-name option
dnl
dnl DESCRIPTION
dnl
dnl   This macro tries to find the Oracle libraries and header files.
dnl
dnl   It defines two options:
dnl   --with-oracle=yes/no/<directory>
dnl   --with-oracle-libdir-name=<dir. name>
dnl
dnl   If the 1st option is "yes" then the macro in several well known directories
dnl
dnl   If the 1st option is "no" then the macro does not attempt at locating the
dnl   oracle package
dnl
dnl   If the 1st option is a drectory name, then the macro tries to locate the oracle package
dnl   in the specified directory.
dnl
dnl   If the macro has to try to locate the oracle package in one or more directories, it will
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
dnl    AC_SUBST(ORACLE_LIBS)
dnl    AC_SUBST(ORACLE_CFLAGS)
dnl    oracle_found=yes/no
dnl
dnl   and if the oracle package is found:
dnl
dnl    AM_CONDITIONAL(ORACLE, true)
dnl
dnl
dnl LICENSE
dnl
dnl This file is free software; the author(s) gives unlimited
dnl permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.
dnl

m4_define([_ORACLE_CHECK_INTERNAL],
[
    AC_BEFORE([AC_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([AM_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([LT_INIT],[$0])dnl setup libtool first

    oracle_loclibdir=$1
    if test "x$oracle_loclibdir" = x
    then
        if test "x$platform_win32" = xyes
	then
	    oracle_loclibdir=bin
	else
	    oracle_loclibdir=lib
	fi
    fi

    # determine if Oracle should be searched for
    oracle_found=no
    try_oracle=true
    ORACLE_LIBS=""
    oracle_test_dir="$ORACLE_HOME /usr /opt/oracle /local"
    AC_ARG_WITH(oracle,
              AS_HELP_STRING([--with-oracle[=@<:@yes/no/<directory>@:>@]],
                             [Locate Oracle's client libraries]),[
			     if test $withval = no
			     then
			         try_oracle=false
			     elif test $withval != yes
			     then
			         oracle_test_dir=$withval
			     fi])
    AC_ARG_WITH(oracle-libdir-name,
              AS_HELP_STRING([--with-oracle-libdir-name[=@<:@<dir. name>@:>@]],
                             [Locate ORACLE library file, related to the prefix specified from --with-oracle]),
			     [oracle_loclibdir=$withval])

    # try to locate files
    if test $try_oracle = true
    then
	if test "x$linklibext" = x
	then
	    if test $platform_win32 = yes
	    then
	        oracle_libext=".dll"
	    else
	        oracle_libext=".so"
            fi
	else
	    oracle_libext="$linklibext"
	fi
	oracledir=""
	for d in $oracle_test_dir
	do
	    oracledir=""
	    AC_MSG_CHECKING([for Oracle files in $d])
	    if test $platform_win32 = yes
	    then
	        orafname="oci$oracle_libext"
	    else
	        orafname="libclntsh$oracle_libext"
	    fi

	    if test -f $d/$oracle_loclibdir/$orafname
	    then
  	        save_CFLAGS="$CFLAGS"
  	        save_LIBS="$LIBS"
	        if test $platform_win32 = yes
		then
		    CFLAGS="$CFLAGS -I$d/include"
	            LIBS="$LIBS -L$d/$oracle_loclibdir -lm -loci"
		else
	            CFLAGS="$CFLAGS -I$d/include -I$d/include/oracle/client -I$d/rdbms/demo -I${ORACLE_HOME}/rdbms/public -I${ORACLE_HOME}/plsql/public -I$d/network/public"
		    LIBS="$LIBS -L$d/$oracle_loclibdir -lm -ldl -lnnz11 -lclntsh"
		fi
   	        AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <oci.h>
int main() {
    printf("%p", OCIInitialize);
    return 0;
}
])],
	                     oracledir=$d)
	        CFLAGS="$save_CFLAGS"
  	        LIBS="$save_LIBS"
 	    fi

	    if test x$oracledir != x
	    then
		AC_MSG_RESULT([found])
	        if test $platform_win32 = yes
		then
		    ORACLE_CFLAGS="-I${oracledir}/include"
	    	    ORACLE_LIBS="-L${oracledir}/$oracle_loclibdir -lm -loci"
		else
		    ORACLE_CFLAGS="-I${oracledir}/include -I${oracledir}/include/oracle/client -I${oracledir}/rdbms/demo -I${ORACLE_HOME}/rdbms/public -I${ORACLE_HOME}/plsql/public -I${oracledir}/network/public"
		    ORACLE_LIBS="-L${oracledir}/$oracle_loclibdir -lm -ldl -lnnz11 -lclntsh"
		fi
		break
  	    else
	        AC_MSG_RESULT([not found])
	    fi
	done

	if test "x$ORACLE_LIBS" = x
	then
	    AC_MSG_NOTICE([ORACLE backend not used])
	else
    	    oracle_found=yes
	fi
    fi

    AM_CONDITIONAL(ORACLE,[test "$oracle_found" = "yes"])
    AC_SUBST(ORACLE_LIBS)
    AC_SUBST(ORACLE_CFLAGS)
])

dnl Usage:
dnl   ORACLE_CHECK([libdirname])

AC_DEFUN([ORACLE_CHECK],
[
    _ORACLE_CHECK_INTERNAL([$1])
])
