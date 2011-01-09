lib_LTLIBRARIES = libgda-report-4.0.la

SUBDIRS = engine DocBook RML

AM_CPPFLAGS = \
	-I$(top_builddir)/libgda-report \
	-I$(top_srcdir)/libgda-report \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS) 

report_headers = \
	gda-report-document.h

libgda_report_4_0_la_SOURCES = \
	$(report_headers) \
	gda-report-document-private.h \
	gda-report-document.c

libgda_report_4_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) \
	$(NO_UNDEFINED) $(LIBTOOL_EXPORT_OPTIONS)
libgda_report_4_0_la_LIBADD = engine/libgda-report-engine-4.0.la \
	DocBook/libgda-report-docbook-4.0.la \
	RML/libgda-report-rml-4.0.la \
	$(LIBGDA_LIBS)

if PLATFORM_WIN32
libgda_report_4_0_la_LDFLAGS += -export-symbols $(srcdir)/libgda-report.def
libgda_report_4_0_la_DEPENDENCIES = libgda-report.def

libgda-report.def: libgda-report.symbols
	(echo -e EXPORTS; $(CPP) -P - <$(srcdir)/libgda-report.symbols | sed -e '/^$$/d' -e 's/^/ /' -e 's/G_GNUC_[^ ]*//g' | sort) > libgda-report.def.tmp && mv libgda-report.def.tmp libgda-report.def

libgda-report-$(GDA_ABI_VERSION).lib: libgda-report.def
	$(DLLTOOL) --dllname libgda-report-$(GDA_ABI_VERSION)-$(GDA_ABI_MAJOR_VERSION).dll --def libgda-report.def --output-lib libgda-report-$(GDA_ABI_VERSION).lib

install-data-local: install-def-files
uninstall-local: uninstall-def-files

install-def-files: libgda-report.def libgda-report-$(GDA_ABI_VERSION).lib
	$(INSTALL) $(srcdir)/libgda-report.def $(DESTDIR)$(libdir)/libgda-report-$(GDA_ABI_VERSION).def
	$(INSTALL) $(srcdir)/libgda-report-$(GDA_ABI_VERSION).lib $(DESTDIR)$(libdir)/

uninstall-def-files:
	-rm $(DESTDIR)$(libdir)/libgda-report-$(GDA_ABI_VERSION).def
	-rm $(DESTDIR)$(libdir)/libgda-report-$(GDA_ABI_VERSION).lib
endif

gdareportincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda-report
gdareportinclude_HEADERS= libgda-report.h $(report_headers)

CLEANFILES = libgda-report.def \
	libgda-report-$(GDA_ABI_VERSION).lib

EXTRA_DIST = libgda-report.symbols
