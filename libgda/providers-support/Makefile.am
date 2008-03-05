noinst_LTLIBRARIES = libgda-psupport.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) 

psupport_headers = \
	gda-pmodel.h \
	gda-prow.h \
	gda-pstmt.h

libgda_psupport_la_SOURCES = \
	$(psupport_headers) \
	gda-pmodel.c \
	gda-prow.c \
	gda-pstmt.c

libgda_psupport_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_psupport_la_LIBADD = $(LIBGDA_LIBS)

psupportincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers-support
psupportinclude_HEADERS=$(psupport_headers)