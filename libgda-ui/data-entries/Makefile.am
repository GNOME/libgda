noinst_LTLIBRARIES = libgda-ui-data-entries.la

SUBDIRS = 

AM_CPPFLAGS = \
        -I$(top_srcdir) -I$(top_srcdir)/libgda -I$(srcdir)/.. \
        -I$(top_builddir) -I$(top_builddir)/libgda-ui \
        $(LIBGDA_CFLAGS) \
        $(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
        -DPREFIX=\""$(prefix)"\" \
        -DSYSCONFDIR=\""$(sysconfdir)"\" \
        -DDATADIR=\""$(datadir)"\" \
        -DLIBDIR=\""$(libdir)"\"

gdauiincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda-ui
gdauiinclude_HEADERS= $(extra_headers)
extra_headers = \
	gdaui-data-cell-renderer-util.h

libgda_ui_data_entries_headers = \
	gdaui-data-cell-renderer-bin.h \
	gdaui-data-cell-renderer-boolean.h \
	gdaui-data-cell-renderer-combo.h \
	gdaui-data-cell-renderer-info.h \
	gdaui-data-cell-renderer-textual.h \
	gdaui-entry-bin.h \
	gdaui-entry-boolean.h \
	gdaui-entry-combo.h \
	gdaui-entry-none.h \
	gdaui-entry-shell.h \
	gdaui-entry-string.h \
	gdaui-entry-number.h \
	gdaui-entry-common-time.h \
	gdaui-entry-time.h \
	gdaui-entry-timestamp.h \
	gdaui-entry-date.h \
	gdaui-entry-wrapper.h \
	gdaui-entry.h \
	gdaui-formatted-entry.h \
	gdaui-numeric-entry.h

libgda_ui_data_entries_la_SOURCES = \
	$(libgda_ui_data_entries_headers) \
	common-bin.c \
	common-bin.h \
	$(extra_headers) \
	gdaui-data-cell-renderer-util.c \
	gdaui-data-cell-renderer-bin.c \
	gdaui-data-cell-renderer-boolean.c \
	gdaui-data-cell-renderer-combo.c \
	gdaui-data-cell-renderer-info.c \
	gdaui-data-cell-renderer-textual.c \
	gdaui-entry-bin.c \
	gdaui-entry-boolean.c \
	gdaui-entry-combo.c \
	gdaui-entry-none.c \
	widget-embedder.c \
	widget-embedder.h \
	gdaui-entry-shell.c \
	gdaui-entry-string.c \
	gdaui-entry-number.c \
	gdaui-entry-common-time.c \
	gdaui-entry-time.c \
	gdaui-entry-timestamp.c \
	gdaui-entry-date.c \
	gdaui-entry-wrapper.c \
	gdaui-entry.c \
	gdaui-formatted-entry.c \
	gdaui-numeric-entry.c

xmldir   = $(datadir)/libgda-4.0/ui
xml_in_files = \
	gdaui-entry-string.xml.in \
	gdaui-entry-number.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)
