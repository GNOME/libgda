noinst_LTLIBRARIES = libgda-ui-marshallers.la

AM_CPPFLAGS = \
	-I$(top_builddir)/ui \
	-I$(top_srcdir)/ui \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS)

gdaui-marshal.h: gdaui-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_gdaui_marshal > $@
gdaui-marshal.c: gdaui-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --prefix=_gdaui_marshal > $@

libgda_ui_marshallers_la_SOURCES = \
	gdaui-marshal.h \
	gdaui-marshal.c \
	gdaui-custom-marshal.c \
	gdaui-custom-marshal.h

$(OBJECTS) $(libgda_ui_marshallers_la_OBJECTS): gdaui-marshal.c gdaui-marshal.h

EXTRA_DIST = gdaui-marshal.list

CLEANFILES = \
	gdaui-marshal.c \
	gdaui-marshal.h
