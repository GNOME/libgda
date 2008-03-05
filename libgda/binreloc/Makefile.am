## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(BINRELOC_CFLAGS) \
	-DLIBGDAPREFIX=\""$(prefix)"\"

noinst_LTLIBRARIES = libgda_binreloc-4.0.la

libgda_binreloc_4_0_la_SOURCES = \
	gda-binreloc.c \
	gda-binreloc.h 

EXTRA_DIST = binreloc.c binreloc.h
