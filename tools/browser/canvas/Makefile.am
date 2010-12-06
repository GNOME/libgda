noinst_LTLIBRARIES = libcanvas.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/tools/browser \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GOOCANVAS_CFLAGS) \
	$(GRAPHVIZ_CFLAGS) \
	$(MAC_INTEGRATION_CFLAGS)

libcanvas_la_SOURCES = \
	browser-canvas.c \
	browser-canvas.h \
	browser-canvas-priv.h \
	browser-canvas-column.c \
	browser-canvas-column.h \
	browser-canvas-db-relations.c \
	browser-canvas-db-relations.h \
	browser-canvas-decl.h \
	browser-canvas-fkey.c \
	browser-canvas-fkey.h \
	browser-canvas-item.c \
	browser-canvas-item.h \
	browser-canvas-print.c \
	browser-canvas-print.h \
	browser-canvas-table.c \
	browser-canvas-table.h \
	browser-canvas-text.c \
	browser-canvas-text.h \
	browser-canvas-utility.c \
	browser-canvas-utility.h

libcanvas_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) \
	$(GTK_LIBS) \
	$(GOOCANVAS_LIBS) \
	$(GRAPHVIZ_LIBS) \
	$(MAC_INTEGRATION_LIBS)
