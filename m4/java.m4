dnl -*- mode: autoconf -*-
dnl Copyright 2010 Vivien Malerba
dnl
dnl SYNOPSIS
dnl
dnl   JAVA_CHECK([libdirname])
dnl
dnl   [libdirname]: defaults to "lib". Can be overridden by the --with-java-libdir-name option
dnl
dnl DESCRIPTION
dnl
dnl   This macro tries to find the Java compiler and JNI header files. 
dnl
dnl   It defines one option:
dnl   --with-java=yes/no/<directory>
dnl
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
dnl    AC_SUBST(JAVA_LD_PATH)
dnl    AC_SUBST(JFLAGS)
dnl    AC_SUBST(JAVA_LIBS)
dnl    AC_SUBST(JAVA_CFLAGS)
dnl    java_found=yes/no
dnl
dnl   and if the java package is found:
dnl
dnl    AM_CONDITIONAL(JAVA, true)
dnl
dnl
dnl LICENSE
dnl
dnl This file is free software; the author(s) gives unlimited
dnl permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.
dnl

## RUN_JAVA(variable for the result, parameters)
## ----------
## runs the java interpreter ${JAVA_PROG} with specified parameters and
## saves the output to the supplied variable. The exit value is ignored.
AC_DEFUN([RUN_JAVA],
[
  acx_java_result=
  if test -z "${JAVA_PROG}"
  then
      echo "$as_me:$LINENO: JAVA_PROG is not set, cannot run java $2" >&AS_MESSAGE_LOG_FD
  else
      echo "$as_me:$LINENO: running ${JAVA_PROG} $2" >&AS_MESSAGE_LOG_FD
      acx_java_result=`${JAVA_PROG} $2 2>&AS_MESSAGE_LOG_FD`
      echo "$as_me:$LINENO: output: '$acx_java_result'" >&AS_MESSAGE_LOG_FD
  fi
  $1=$acx_java_result
])

m4_define([_JAVA_CHECK_INTERNAL],
[
    AC_BEFORE([AC_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([AM_PROG_LIBTOOL],[$0])dnl setup libtool first
    AC_BEFORE([LT_INIT],[$0])dnl setup libtool first

    # determine if Java should be searched for
    java_found=no
    try_java=true
    JAVA_LIBS=""
    java_test_dir="$JAVA_HOME /usr /opt /local"
    AC_ARG_WITH(java,
              AS_HELP_STRING([--with-java[=@<:@yes/no/<directory>@:>@]],
                             [Locate Java's client libraries]),[
			     if test $withval = no
			     then
			         try_java=false
			     elif test $withval = yes
			     then
			         if test -z "${JAVA_HOME}"
				 then
	  			     JAVA_PATH=${PATH}
				 else
  	  			     JAVA_PATH=${JAVA_HOME}:${JAVA_HOME}/bin:${PATH}
				 fi
				 JAVA_PATH=${JAVA_PATH}:/usr/java/bin:/usr/jdk/bin:/usr/lib/java/bin:/usr/lib/jdk/bin:/usr/local/java/bin:/usr/local/jdk/bin:/usr/local/lib/java/bin:/usr/local/lib/jdk/bin
			     else
			         JAVA_PATH=$withval
			         java_test_dir=$withval
			     fi], [
			     if test -z "${JAVA_HOME}"
			     then
	  		         JAVA_PATH=${PATH}
			     else
  	  		         JAVA_PATH=${JAVA_HOME}:${JAVA_HOME}/bin:${PATH}
			     fi
			     JAVA_PATH=${JAVA_PATH}:/usr/java/bin:/usr/jdk/bin:/usr/lib/java/bin:/usr/lib/jdk/bin:/usr/local/java/bin:/usr/local/jdk/bin:/usr/local/lib/java/bin:/usr/local/lib/jdk/bin
                             ])

    save_CFLAGS="$CFLAGS"
    save_LIBS="$LIBS"
    save_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"

    # try to locate all the java binaries
    if test $try_java = true
    then
	## FIXME: we may want to check for jikes, kaffe and others...
	AC_MSG_CHECKING([if all java programs are found])
	AC_PATH_PROGS(JAVA_PROG,java,,${JAVA_PATH})
	AC_PATH_PROGS(JAVAC,javac,,${JAVA_PATH})
	AC_PATH_PROGS(JAR,jar,,${JAVA_PATH})

	have_all_java=yes
	if test -z "$JAVA_PROG"; then have_all_java=no; fi
	if test -z "$JAVAC"; then have_all_java=no; fi
	if test -z "$JAR"; then have_all_java=no; fi
	if test ${have_all_java} = no; then
	    AC_MSG_WARN([one or more Java tools are missing (JRE is not sufficient)])
    	    try_java=false
	fi
    fi

    # Check if the Java interpreter works
    if test $try_java = true
    then
        AC_MSG_CHECKING([whether Java interpreter works])
	try_java=false
	acx_java_works=no
	if test -n "${JAVA_PROG}"
	then
	    RUN_JAVA(acx_jc_result,[-classpath ${srcdir} getsp -test])
	    if test "${acx_jc_result}" = "Test1234OK"
	    then
	        acx_java_works=yes
	    fi
	fi

	if test ${acx_java_works} = yes
	then
	    AC_MSG_RESULT([yes])
	    AC_MSG_CHECKING([java compiler version])
	    JTYPE=`$JAVAC -version 2>&1 | grep -o Unrecognized`
	    if test x$JTYPE = xUnrecognized
	    then
	        # eclipse gjc?
	        JVERSION=ECJ`$JAVAC -v 2>&1 | grep -o '[[0-9]].[[0-9]].[[0-9]] release' | sed -e 's/ release//'`
	    else
	       JTYPE=`$JAVAC -version 2>&1 | grep -o Eclipse`
	       if test x$JTYPE = xEclipse
	       then
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
                JRE1.7.*)
		    try_java=true
		    JTYPE="Sun JRE 1.7"
		    JFLAGS="-Xlint:unchecked -Xlint:deprecation"
		    ;;
                JRE1.8.*)
		    try_java=true
		    JTYPE="Sun JRE 1.8"
		    JFLAGS="-Xlint:unchecked -Xlint:deprecation"
		    ;;
                JRE11.0.*)
		    try_java=true
		    JTYPE="Sun JRE 11.0"
		    JFLAGS="-Xlint:unchecked -Xlint:deprecation"
		    ;;
                JREgcj-4*)
		    try_java=true
		    JTYPE="GCJ"
		    JFLAGS=""
		    ;;
                ECJ*)
		    try_java=no
		    ;;
                *)
	            ;;
	    esac
	    if test $try_java = true; then
	        AC_MSG_RESULT([JAVA version used is $JTYPE ])

	        AC_MSG_CHECKING([for Java environment])

	        ## retrieve JAVA_HOME from Java itself if not set
	        if test -z "${JAVA_HOME}"
	        then
	            RUN_JAVA(JAVA_HOME,[-classpath ${srcdir} getsp java.home])
	        fi

	        ## the availability of JAVA_HOME will tell us whether it's supported
	        if test -z "${JAVA_HOME}"
	        then
	            if test x$acx_java_env_msg != xyes
		    then
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
	                    RUN_JAVA(JAVA_LIBS, [-classpath ${srcdir} getsp -libs])
	                    JAVA_LIBS="${JAVA_LIBS} -L${JAVA_HOME}/lib/server -ljvm"
			    RUN_JAVA(JAVA_LD_PATH, [-classpath ${srcdir} getsp -ldpath])
	           	    ;;
	            esac
	            # note that we actually don't test JAVA_LIBS - we hope that the detection
	            # was correct. We should also test the functionality for javac.
	        fi
	    else
	        AC_MSG_RESULT([SUN Java compiler Version >= 1.5 required, JDBC provider not compiled])
	    fi
	else
	    AC_MSG_RESULT([no])
	    AC_MSG_WARN([Java not found. Please install JDK 1.5 or later, make sure that the binaries are on the PATH and re-try. If that doesn't work, set JAVA_HOME correspondingly.])
	    try_java=false
	fi
    fi

    # test for JNI
    JNI_H=
    AC_ARG_WITH(jni,
		AS_HELP_STRING([--with-jni[=@<:@<directory>@:>@]],
                             [use JNI headers in <directory>]),
			     [JNI_H=$withval])

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
	for mddir in . linux solaris ppc irix alpha aix hp-ux genunix cygwin win32 freebsd
	do
	    AC_CHECK_FILE(${JNI_H}/$mddir/jni_md.h,[JAVA_CFLAGS="${JAVA_CFLAGS} -I${JNI_H}/$mddir" jac_found_md=yes])
	    if test ${jac_found_md} = yes
	    then
	        break
	    fi
	done

	LIBS="${LIBS} ${JAVA_LIBS}"
	CFLAGS="${CFLAGS} ${JAVA_CFLAGS} ${JAVA_CFLAGS}"

	AC_MSG_CHECKING([whether JNI programs can be compiled])
	AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <jni.h>
int main(void) {
    jobject o;
    o = NULL;
    return 0;
}])],
	               [AC_MSG_RESULT(yes)],
	      	       [AC_MSG_ERROR([Cannot compile a simple JNI program. See config.log for details.])])

	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${JAVA_LD_PATH}
	export LD_LIBRARY_PATH

	AC_MSG_CHECKING([whether JNI programs can be run])
	AC_RUN_IFELSE([AC_LANG_SOURCE([
#include <jni.h>
int main(void) {
    jobject o;
    o = NULL;
    return 0;
}])],
	              [AC_MSG_RESULT(yes)],
	      	      [AC_MSG_ERROR([Cannot run a simple JNI program - probably your jvm library is in non-standard location or JVM is unsupported. See config.log for details.])])

	AC_MSG_CHECKING([JNI data types])
	AC_RUN_IFELSE([AC_LANG_SOURCE([
#include <jni.h>
int main(void) {
  return (sizeof(int)==sizeof(jint) && sizeof(long)==sizeof(long) && sizeof(jbyte)==sizeof(char) && sizeof(jshort)==sizeof(short) && sizeof(jfloat)==sizeof(float) && sizeof(jdouble)==sizeof(double))?0:1;
}])],
	              [AC_MSG_RESULT([ok])],
		      [AC_MSG_ERROR([One or more JNI types differ from the corresponding native type. You may need to use non-standard compiler flags or a different compiler in order to fix this.])],[])

	JAVA_CFLAGS="-I${JNI_H}"
	for mddir in . linux solaris ppc irix alpha aix hp-ux genunix cygwin win32 freebsd
	do
	    if test -e ${JNI_H}/$mddir/jni_md.h
	    then
		JAVA_CFLAGS="${JAVA_CFLAGS} -I${JNI_H}/$mddir"
		break
	    fi
	done
	
	java_found=yes
	AC_SUBST(JAVA_LD_PATH)
	AC_SUBST(JFLAGS)
	AC_SUBST(JAVA_LIBS)
	AC_SUBST(JAVA_CFLAGS)
    fi

    AM_CONDITIONAL(JAVA, test x$java_found = xyes)
    CFLAGS="$save_CFLAGS"
    LIBS="$save_LIBS"
    export LD_LIBRARY_PATH="$save_LD_LIBRARY_PATH"
])

dnl Usage:
dnl   JAVA_CHECK([libdirname])

AC_DEFUN([JAVA_CHECK],
[
    _JAVA_CHECK_INTERNAL()
])
