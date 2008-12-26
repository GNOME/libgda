## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(BINRELOC_CFLAGS) \
	-DLIBGDAPREFIX=\""$(prefix)"\" \
	-DLIBGDADATA=\""$(datadir)"\" \
	-DLIBGDALIB=\""$(libdir)"\" \
	-DLIBGDALIBEXEC=\""$(libexecdir)"\" \
	-DLIBGDABIN=\""$(bindir)"\" \
	-DLIBGDASBIN=\""$(sbindir)"\" \
	-DLIBGDASYSCONF=\""$(sysconfdir)"\"

noinst_LTLIBRARIES = libgdasql_binreloc-4.0.la

libgdasql_binreloc_4_0_la_SOURCES = \
	sql-binreloc.h \
	sql-binreloc.c

EXTRA_DIST = binreloc.c binreloc.h
