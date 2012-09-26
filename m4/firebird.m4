dnl -*- mode: autoconf -*-
dnl Copyright 2010 Vivien Malerba
dnl
dnl SYNOPSIS
dnl
dnl   FIREBIRD_CHECK([libdirname])
dnl
dnl   [libdirname]: defaults to "lib". Can be overridden by the --with-firebird-libdir-name option
dnl
dnl DESCRIPTION
dnl
dnl   This macro tries to find the Firebird libraries and header files.
dnl
dnl   It defines two options:
dnl   --with-firebird=yes/no/<directory>
dnl   --with-firebird-libdir-name=<dir. name>
dnl
dnl   If the 1st option is "yes" then the macro in several well known directories
dnl
dnl   If the 1st option is "no" then the macro does not attempt at locating the
dnl   firebird package
dnl
dnl   If the 1st option is a drectory name, then the macro tries to locate the firebird package
dnl   in the specified directory.
dnl
dnl   If the macro has to try to locate the firebird package in one or more directories, it will
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
dnl    AC_SUBST(FIREBIRD_CLIENT_LIBS)
dnl    AC_SUBST(FIREBIRD_CLIENT_CFLAGS)
dnl    firebird_client_found=yes/no
dnl    AC_SUBST(FIREBIRD_EMBED_LIBS)
dnl    AC_SUBST(FIREBIRD_EMBED_CFLAGS)
dnl    firebird_embed_found=yes/no
dnl
dnl   and if the firebird package is found:
dnl
dnl    AM_CONDITIONAL(FIREBIRD, true)
dnl    AM_CONDITIONAL(FIREBIRD_CLIENT, true)
dnl    AM_CONDITIONAL(FIREBIRD_EMBED, true)
dnl
dnl
dnl LICENSE
dnl
dnl This file is free software; the author(s) gives unlimited
dnl permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.
dnl

m4_define([_FIREBIRD_CHECK_INTERNAL],
[
    AC_BEFORE([AC_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([AM_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([LT_INIT],[$0])dnl setup libtool first

    firebird_loclibdir=$1
    if test "x$firebird_loclibdir" = x
    then
        if test "x$platform_win32" = xyes
	then
	    firebird_loclibdir=bin
	else
	    firebird_loclibdir=lib
	fi
    fi

    firebird_client_found=no
    firebird_embed_found=no

    # determine if Firebird should be searched for
    firebird_found=no
    try_firebird=true
    pkgfirebird=no
    firebird_test_dir="$FIREBIRD_HOME /usr /opt/firebird /local"
    AC_ARG_WITH(firebird,
              AS_HELP_STRING([--with-firebird[=@<:@yes/no/<directory>@:>@]],
                             [Locate Firebird's client libraries]),[
			     if test $withval = no
			     then
			         try_firebird=false
			     elif test $withval != yes
			     then
			         firebird_test_dir=$withval
			     fi])
    AC_ARG_WITH(firebird-libdir-name,
              AS_HELP_STRING([--with-firebird-libdir-name[=@<:@<dir. name>@:>@]],
                             [Locate FIREBIRD library file, related to the prefix specified from --with-firebird]),
			     [firebird_loclibdir=$withval])

    # try with pkgconfig
    if test $try_firebird = true
    then
        PKG_CHECK_MODULES(FIREBIRD_CLIENT, "fbclient",[pkgfbclient=yes],[pkgfbclient=no])
        PKG_CHECK_MODULES(FIREBIRD_EMBED, "fbembed",[pkgfbembed=yes],[pkgfbembed=no])
        if test $pkgfbclient = yes -o $pkgfbembed = yes
        then
	    pkgfirebird=yes
        fi
	if test $pkgfbclient = yes
	then
	    firebird_client_found=yes
	fi
	if test $pkgfbembed = yes
	then
	    firebird_embed_found=yes
	fi
    fi

    # try to locate files
    if test $try_firebird = true -a $pkgfirebird = no
    then
	if test "x$linklibext" = x
	then
	    if test $platform_win32 = yes
	    then
	        firebird_libext=".dll"
	    else
	        firebird_libext=".so"
            fi
	else
	    firebird_libext="$linklibext"
	fi

	dnl checking for client libraries
	firebirddir=""
	for d in $firebird_test_dir
	do
	    firebirddir=""
	    AC_MSG_CHECKING([for Firebird client files in $d])
	    if test $platform_win32 = yes
	    then
	        clntlibname="fbclient$firebird_libext"
	    else
	        clntlibname="libfbclient$firebird_libext"
	    fi

	    if test -f $d/$firebird_loclibdir/$clntlibname
	    then
  	        save_CFLAGS="$CFLAGS"
  	        save_LIBS="$LIBS"
		CFLAGS="$CFLAGS -I$d/include"
	        LIBS="$LIBS -L$d/$firebird_loclibdir -lfbclient"
   	        AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <ibase.h>
int main() {
    printf("%p", isc_open);
    return 0;
}
])],
	                     firebirddir=$d)
	        CFLAGS="$save_CFLAGS"
  	        LIBS="$save_LIBS"
 	    fi

	    if test x$firebirddir != x
	    then
		AC_MSG_RESULT([found])
		FIREBIRD_CLIENT_CFLAGS="-I${firebirddir}/include"
	    	FIREBIRD_CLIENT_LIBS="-L${firebirddir}/$firebird_loclibdir -lfbclient"
		break
  	    else
	        AC_MSG_RESULT([not found])
	    fi
	done

	dnl checking for embedded libraries
	firebirddir=""
	for d in $firebird_test_dir
	do
	    firebirddir=""
	    AC_MSG_CHECKING([for Firebird embedded library files in $d])
	    if test $platform_win32 = yes
	    then
	        clntlibname="fbembed$firebird_libext"
	    else
	        clntlibname="libfbembed$firebird_libext"
	    fi

	    if test -f $d/$firebird_loclibdir/$clntlibname
	    then
  	        save_CFLAGS="$CFLAGS"
  	        save_LIBS="$LIBS"
		CFLAGS="$CFLAGS -I$d/include"
	        LIBS="$LIBS -L$d/$firebird_loclibdir -lfbembed"
   	        AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <ibase.h>
int main() {
    printf("%p", isc_open);
    return 0;
}
])],
	                     firebirddir=$d)
	        CFLAGS="$save_CFLAGS"
  	        LIBS="$save_LIBS"
 	    fi

	    if test x$firebirddir != x
	    then
		AC_MSG_RESULT([found])
		FIREBIRD_EMBED_CFLAGS="-I${firebirddir}/include"
	    	FIREBIRD_EMBED_LIBS="-L${firebirddir}/$firebird_loclibdir -lfbembed"
		break
  	    else
	        AC_MSG_RESULT([not found])
	    fi
	done


	if test "x$FIREBIRD_CLIENT_LIBS" = x -a "x$FIREBIRD_EMBED_LIBS"
	then
	    AC_MSG_NOTICE([FIREBIRD backend not used])
	else
	    if test "x$FIREBIRD_CLIENT_LIBS" != x
    	    then
	        firebird_client_found=yes
	    fi
	    if test "x$FIREBIRD_EMBED_LIBS" != x
    	    then
	        firebird_embed_found=yes
	    fi
	fi
    fi

    AM_CONDITIONAL(FIREBIRD,[test "$firebird_client_found" = "yes" -o "$firebird_embed_found" = "yes"])
    AM_CONDITIONAL(FIREBIRD_CLIENT,[test "$firebird_client_found" = "yes"])
    AM_CONDITIONAL(FIREBIRD_EMBED,[test "$firebird_embed_found" = "yes"])
    AC_SUBST(FIREBIRD_CLIENT_LIBS)
    AC_SUBST(FIREBIRD_CLIENT_CFLAGS)
    AC_SUBST(FIREBIRD_EMBED_LIBS)
    AC_SUBST(FIREBIRD_EMBED_CFLAGS)
])

dnl Usage:
dnl   FIREBIRD_CHECK([libdirname])

AC_DEFUN([FIREBIRD_CHECK],
[
    _FIREBIRD_CHECK_INTERNAL([$1])
])
