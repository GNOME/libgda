AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-test-connection-5.0
if HAVE_UI
UI_PROGS=gdaui-test-data-entries gdaui-test-widget-entry gdaui-test-errors gdaui-test-rt-editor
endif
noinst_PROGRAMS = gda-test-blob gda-provider-status virtual-test virtual-test-2 $(UI_PROGS)

gda_test_connection_5_0_SOURCES = \
        gda-test-connection.c

gda_test_connection_5_0_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(LIBGDA_LIBS)


gda_test_blob_SOURCES = \
        gda-test-blob.c

gda_test_blob_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(LIBGDA_LIBS)


gda_provider_status_SOURCES = \
	html.h \
	html.c \
        gda-provider-status.c

gda_provider_status_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(LIBGDA_LIBS)

virtual_test_SOURCES = \
	virtual-test.c

virtual_test_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(LIBGDA_LIBS)

virtual_test_2_SOURCES = \
	virtual-test-2.c

virtual_test_2_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(LIBGDA_LIBS)

gdaui_test_data_entries_CFLAGS = $(GTK_CFLAGS)
gdaui_test_data_entries_SOURCES = \
        gdaui-test-data-entries.c

gdaui_test_data_entries_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(top_builddir)/libgda-ui/libgda-ui-5.0.la \
        $(LIBGDA_LIBS) \
	$(GTK_LIBS)

gdaui_test_widget_entry_CFLAGS = $(GTK_CFLAGS)
gdaui_test_widget_entry_SOURCES = \
        gdaui-test-widget-entry.c

gdaui_test_widget_entry_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(top_builddir)/libgda-ui/libgda-ui-5.0.la \
        $(LIBGDA_LIBS) \
	$(GTK_LIBS)

gdaui_test_errors_CFLAGS = $(GTK_CFLAGS)
gdaui_test_errors_SOURCES = \
        gdaui-test-errors.c

gdaui_test_errors_LDADD = \
        $(top_builddir)/libgda/libgda-5.0.la \
        $(top_builddir)/libgda-ui/libgda-ui-5.0.la \
	$(top_builddir)/tests/libgda-test-5.0.la \
        $(LIBGDA_LIBS) \
	$(GTK_LIBS)

gdaui_test_rt_editor_CFLAGS = $(GTK_CFLAGS)
gdaui_test_rt_editor_SOURCES = \
        gdaui-test-rt-editor.c

gdaui_test_rt_editor_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(top_builddir)/libgda-ui/libgda-ui-4.0.la \
        $(LIBGDA_LIBS) \
	$(GTK_LIBS)

EXTRA_DIST = 
