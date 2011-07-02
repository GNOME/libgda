dnl -*- mode: autoconf -*-
dnl Copyright 2010 Vivien Malerba
dnl
dnl SYNOPSIS
dnl
dnl   LDAP_CHECK([libdirname])
dnl
dnl   [libdirname]: defaults to "lib". Can be overridden by the --with-bdb-libdir-name option
dnl
dnl DESCRIPTION
dnl
dnl   This macro tries to find the LDAP libraries and header files
dnl
dnl   It defines two options:
dnl   --with-ldap=yes/no/<directory>
dnl   --with-ldap-libdir-name=<dir. name>
dnl
dnl   If the 1st option is "yes" then the macro in several well known directories
dnl
dnl   If the 1st option is "no" then the macro does not attempt at locating the
dnl   LDAP package
dnl
dnl   If the 1st option is a directory name, then the macro tries to locate the LDAP package
dnl   in the specified directory.
dnl
dnl   If the macro has to try to locate the LDAP package in one or more directories, it will
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
dnl    AC_SUBST(LDAP_LIBS)
dnl    AC_SUBST(LDAP_LIB)
dnl    AC_SUBST(LDAP_CFLAGS)
dnl    AC_SUBST(LIBGDA_LDAP_INC)
dnl    AC_SUBST(LIBGDA_LDAP_INC2)
dnl    AC_SUBST(LIBGDA_LDAP_VINC)
dnl    AC_SUBST(LIBGDA_LDAP_TYPE)
dnl    AC_SUBST(LIBGDA_LDAP_TYPE2)
dnl    AC_SUBST(LIBGDA_LDAP_TYPE3)
dnl    ldap_found=yes/no
dnl
dnl   and if the ldap package is found:
dnl
dnl    AM_CONDITIONAL(LDAP, true)
dnl
dnl
dnl LICENSE
dnl
dnl This file is free software; the author(s) gives unlimited
dnl permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.
dnl

m4_define([_LDAP_CHECK_INTERNAL],
[
    AC_BEFORE([AC_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([AM_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([LT_INIT],[$0])dnl setup libtool first

    ldap_loclibdir=$1
    if test "x$ldap_loclibdir" = x
    then
        if test "x$platform_win32" = xyes
	then
	    ldap_loclibdir=bin
	else
	    ldap_loclibdir=lib
	fi
    fi

    # determine if Ldap should be searched for
    # and use pkg-config if the "yes" option is used
    ldap_found=no
    try_ldap=true
    LDAP_LIBS=""
    ldap_test_dir="/usr /usr/local /local"
    AC_ARG_WITH(ldap,
              AS_HELP_STRING([--with-ldap[=@<:@yes/no/<directory>@:>@]],
                             [Locate LDAP client library]),[
			     if test $withval = no
			     then
			         try_ldap=false
			     elif test $withval != yes
			     then
			         ldap_test_dir=$withval
			     fi])
    AC_ARG_WITH(ldap-libdir-name,
              AS_HELP_STRING([--with-ldap-libdir-name[=@<:@<dir. name>@:>@]],
                             [Locate LDAP library file, related to the prefix specified from --with-ldap]),
			     [ldap_loclibdir=$withval])

    # try to locate files
    if test $try_ldap = true
    then
	if test "x$linklibext" = x
	then
	    ldap_libext=".so"
	else
	    ldap_libext="$linklibext"
	fi
	ldapdir=""
	for d in $ldap_test_dir
	do
	    ldapdir=""
	    AC_MSG_CHECKING([for LDAP files in $d])

	    if test -f $d/include/ldap.h
	    then
  	        save_CFLAGS="$CFLAGS"
	        CFLAGS="$CFLAGS -I$d/include"
  	        save_LIBS="$LIBS"
	        LIBS="$LIBS -L$d/$ldap_loclibdir -lldap -llber"
   	        AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <ldap.h>
#include <lber.h>
#include <ldap_schema.h>
int main() {
    printf("%p,%p", ldap_initialize, ldap_str2attributetype);
    printf("%p", ber_free);
    return 0;
}
])],
	                     ldapdir=$d)
	        CFLAGS="$save_CFLAGS"
  	        LIBS="$save_LIBS"
	    fi

            if test x$ldapdir != x
	    then
	        AC_MSG_RESULT([found])
	        LDAP_CFLAGS=-I${ldapdir}/include
	        LDAP_LIBS="-L${ldapdir}/$ldap_loclibdir -lldap -llber"
		break
  	    else
	        AC_MSG_RESULT([not found])
	    fi
	done

	if test "x$LDAP_LIBS" = x
	then
	    AC_MSG_NOTICE([LDAP backend not used])
	else
	    LIBGDA_LDAP_INC="#include <libgda/gda-data-model-ldap.h>"
	    LIBGDA_LDAP_INC2="#include <libgda/gda-tree-mgr-ldap.h>"
	    LIBGDA_LDAP_VINC="#include <virtual/gda-ldap-connection.h>"
            LIBGDA_LDAP_TYPE="gda_data_model_ldap_get_type"
            LIBGDA_LDAP_TYPE2="gda_ldap_connection_get_type"
            LIBGDA_LDAP_TYPE3="gda_tree_mgr_ldap_get_type"
    	    ldap_found=yes
	fi
    fi

    AM_CONDITIONAL(LDAP,[test "$ldap_found" = "yes"])
    AC_SUBST(LDAP_LIBS)
    AC_SUBST(LDAP_CFLAGS)
    AC_SUBST(LIBGDA_LDAP_INC)
    AC_SUBST(LIBGDA_LDAP_INC2)
    AC_SUBST(LIBGDA_LDAP_VINC)
    AC_SUBST(LIBGDA_LDAP_TYPE)
    AC_SUBST(LIBGDA_LDAP_TYPE2)
    AC_SUBST(LIBGDA_LDAP_TYPE3)
])


dnl Usage:
dnl   LDAP_CHECK([libdirname])

AC_DEFUN([LDAP_CHECK],
[
    _LDAP_CHECK_INTERNAL([$1])
])
