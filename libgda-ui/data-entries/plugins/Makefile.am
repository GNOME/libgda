plugin_LTLIBRARIES = libgda-ui-plugins.la
plugindir = $(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/plugins

AM_CPPFLAGS = \
        -I$(top_srcdir) -I$(top_srcdir)/libgda -I$(srcdir)/.. \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS) \
	$(LIBGDA_WFLAGS) \
        $(GTK_CFLAGS) \
	$(LIBGCRYPT_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
        -DPREFIX=\""$(prefix)"\" \
        -DSYSCONFDIR=\""$(sysconfdir)"\" \
        -DDATADIR=\""$(datadir)"\" \
        -DLIBDIR=\""$(plugindir)"\"

if HAVE_LIBGCRYPT
gcrypt_headers= gdaui-entry-password.h \
		gdaui-data-cell-renderer-password.h
gcrypt_sources= gdaui-entry-password.c \
		gdaui-data-cell-renderer-password.c
else
gcrypt_headers=
gcrypt_sources=
endif

plugins_headers = \
	common-pict.h \
	gdaui-data-cell-renderer-pict.h \
	gdaui-entry-filesel.h \
	gdaui-entry-cidr.h \
	gdaui-entry-format.h \
	gdaui-entry-text.h \
	gdaui-entry-pict.h \
	gdaui-entry-rt.h \
	$(gcrypt_headers)

libgda_ui_plugins_la_SOURCES = \
	$(plugins_headers) \
	gdaui-data-cell-renderer-util.c \
	gdaui-data-cell-renderer-util.h \
	common-pict.c \
	custom-marshal.c \
	custom-marshal.h \
	libmain.c \
	gdaui-data-cell-renderer-pict.c \
	gdaui-entry-filesel.c \
	gdaui-entry-cidr.c \
	gdaui-entry-format.c \
	gdaui-entry-text.c \
	gdaui-entry-pict.c \
	gdaui-entry-rt.c \
	$(gcrypt_sources)

libgda_ui_plugins_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_ui_plugins_la_LIBADD = \
        $(top_builddir)/libgda-ui/libgda-ui-4.0.la \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGCRYPT_LIBS) \
	$(LIBGDA_LIBS) \
        $(GTK_LIBS) \
	$(GTKSOURCEVIEW_LIBS)

xmldir   = $(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/plugins
xml_in_files = \
	gdaui-entry-pict-spec.xml.in \
	gdaui-entry-pict-spec_string.xml.in \
	gdaui-entry-filesel-spec.xml.in \
	gdaui-entry-password.xml.in \
	gdaui-entry-text-spec.xml.in \
	gdaui-entry-format-spec.xml.in

xml_DATA = $(xml_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

langspecdir=$(datadir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/language-specs
langspec_DATA= \
	gda-sql.lang

EXTRA_DIST = $(xml_in_files) \
	$(langspec_DATA)
DISTCLEANFILES = $(xml_DATA)
