## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(LIBGDA_WFLAGS) \
	$(BINRELOC_CFLAGS) \
	-DLIBGDAPREFIX=\""$(prefix)"\" \
	-DLIBGDADATA=\""$(datadir)"\" \
	-DLIBGDALIB=\""$(libdir)"\" \
	-DLIBGDALIBEXEC=\""$(libexecdir)"\" \
	-DLIBGDABIN=\""$(bindir)"\" \
	-DLIBGDASBIN=\""$(sbindir)"\" \
	-DLIBGDASYSCONF=\""$(sysconfdir)"\"

noinst_LTLIBRARIES = libgda_binreloc-4.0.la

binreloc_headers = \
	gda-binreloc.h 

binrelocincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda
binrelocinclude_HEADERS=$(binreloc_headers)

libgda_binreloc_4_0_la_SOURCES = \
	gda-binreloc.c \
	$(binreloc_headers)

EXTRA_DIST = binreloc.c binreloc.h
