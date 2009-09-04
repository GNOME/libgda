bin_PROGRAMS=gda-browser-4.0
noinst_LTLIBRARIES = libbrowser.la

SUBDIRS = data common schema-browser query-exec dummy-perspective
if HAVE_GOOCANVAS
SUBDIRS+=canvas
noinst_PROGRAMS=canvas-example
endif
SUBDIRS+= . doc

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/libgda \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS) \
        $(GTK_CFLAGS) \
        $(GTKSOURCEVIEW_CFLAGS) \
        -DPREFIX=\""$(prefix)"\" \
        -DSYSCONFDIR=\""$(sysconfdir)"\" \
        -DDATADIR=\""$(datadir)"\" \
        -DLIBDIR=\""$(libdir)"\"

marshal.h: marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_marshal > $@
marshal.c: marshal.list $(GLIB_GENMARSHAL) marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_marshal > $@

libbrowser_la_SOURCES=\
	cc-gray-bar.c \
	cc-gray-bar.h \
	marshal.c \
	marshal.h \
	decl.h \
	dnd.h \
	dnd.c \
	auth-dialog.c \
	auth-dialog.h \
	login-dialog.c \
	login-dialog.h \
	support.c \
	support.h \
	browser-core.c \
	browser-core.h \
	browser-page.c \
	browser-page.h \
	browser-perspective.c \
	browser-perspective.h \
	browser-spinner.c \
	browser-spinner.h \
	browser-connection.c \
	browser-connection.h \
	browser-favorites.c \
	browser-favorites.h \
	browser-variable.c \
	browser-variable.h \
	browser-window.c \
	browser-window.h \
	browser-connections-list.c \
	browser-connections-list.h \
	mgr-favorites.h \
	mgr-favorites.c \
	browser-stock-icons.c \
	browser-stock-icons.h

gda_browser_4_0_SOURCES=\
	main.c

$(OBJECTS): marshal.c marshal.h

if PLATFORM_WIN32
EXTRALDFLAGS=-mwindows
gda_browser_4_0_SOURCES += browser-res.rc
else
EXTRALDFLAGS=
endif

if HAVE_GOOCANVAS
CANVAS_LDADD=$(top_builddir)/tools/browser/canvas/libcanvas.la
endif

.rc.o:
	$(WINDRES) $^ -o $@

browser-res.o: browser-res.rc
	$(WINDRES) $^ -o $@

gda_browser_4_0_LDFLAGS = $(EXTRALDFLAGS)
gda_browser_4_0_LDADD=\
	$(top_builddir)/tools/browser/schema-browser/libperspective.la \
	$(top_builddir)/tools/browser/query-exec/libperspective.la \
	$(top_builddir)/tools/browser/dummy-perspective/libperspective.la \
	$(top_builddir)/tools/browser/libbrowser.la \
	$(CANVAS_LDADD) \
	$(top_builddir)/tools/browser/common/libcommon.la \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(top_builddir)/libgda-ui/libgda-ui-4.0.la \
	$(LIBGDA_LIBS) $(GTK_LIBS) $(GTKSOURCEVIEW_LIBS)


@INTLTOOL_DESKTOP_RULE@

desktopdir=$(datadir)/applications
Desktop_in_files = gda-browser-4.0.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

# canvas example
canvas_example_DEPENDENCIES = \
	canvas/libcanvas.la
canvas_example_SOURCES = \
	canvas-example.c \
	dnd.c \
	dnd.h
canvas_example_CFLAGS = -DCANVAS_EXAMPLE
canvas_example_LDFLAGS = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(top_builddir)/libgda-ui/libgda-ui-4.0.la \
	$(top_builddir)/tools/browser/canvas/libcanvas.la \
	$(top_builddir)/tools/browser/common/libcommon.la

# icons
iconsdir=$(datadir)/libgda-4.0/pixmaps
icons_DATA= \
	gda-browser.png \
	gda-browser-auth.png \
	gda-browser-auth-big.png \
	gda-browser-connected.png \
	gda-browser-connected-big.png \
	gda-browser-non-connected.png \
	gda-browser-non-connected-big.png \
	gda-browser-bookmark.png \
	gda-browser-schema.png \
	gda-browser-table.png \
	gda-browser-column.png \
	gda-browser-column-fknn.png \
	gda-browser-column-fk.png \
	gda-browser-column-nn.png \
	gda-browser-column-pk.png \
	gda-browser-reference.png \
	gda-browser-diagram.png

EXTRA_DIST = \
	marshal.list \
	browser-res.rc \
	gda-browser.ico \
	$(Desktop_in_files) $(icons_DATA)

DISTCLEANFILES = $(desktop_DATA)

CLEANFILES = \
	marshal.h \
	marshal.c
