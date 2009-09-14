noinst_LTLIBRARIES = libcommon.la

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-DPRIV_OBJ_PREFIX="\"BROWSER\"" \
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

libcommon_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) \
	$(GTK_LIBS)

EXTRA_DIST = \
	marshal.list

CLEANFILES = \
	marshal.h \
	marshal.c
