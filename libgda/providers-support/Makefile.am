noinst_LTLIBRARIES = libgda-psupport.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) 

libgda_psupport_la_SOURCES = \
	gda-pmodel.h \
	gda-pmodel.c \
	gda-prow.h \
	gda-prow.c \
	gda-pstmt.h \
	gda-pstmt.c

libgda_psupport_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_psupport_la_LIBADD = $(LIBGDA_LIBS)
