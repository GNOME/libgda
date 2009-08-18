noinst_LTLIBRARIES = libcommon.la

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS) \
	$(GTK_CFLAGS)

marshal.h: marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_common_marshal > $@
marshal.c: marshal.list $(GLIB_GENMARSHAL) marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_common_marshal > $@

libcommon_la_SOURCES = \
	marshal.c \
	marshal.h \
	objects-cloud.c \
	objects-cloud.h \
	popup-container.c \
	popup-container.h

$(OBJECTS): marshal.c marshal.h

EXTRA_DIST = \
	marshal.list

CLEANFILES = \
	marshal.h \
	marshal.c
