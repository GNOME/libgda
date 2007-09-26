lib_LTLIBRARIES = libgda-xslt-3.0.la

AM_CPPFLAGS = \
        -I$(top_builddir)/libgda-report \
        -I$(top_srcdir)/libgda-report \
        -I$(top_builddir) \
        -I$(top_srcdir) \
        -I$(top_srcdir)/libgda/sqlite \
        $(LIBGDA_CFLAGS) $(LIBXSLT_CFLAGS)

gda_xslt_headers = \
        libgda-xslt.h

libgda_xslt_3_0_la_SOURCES =   \
        $(gda_xslt_headers)   \
	sql_backend.h \
        sqlexslt.c \
	sql_exslt_internal.c \
	sql_backend.c

libgda_xslt_3_0_la_LIBADD = $(LIBGDA_LIBS) $(LIBXSLT_LIBS)\
        $(top_builddir)/libgda/libgda-3.0.la

gdaxsltincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda-xslt
gdaxsltinclude_HEADERS=$(gda_xslt_headers)
