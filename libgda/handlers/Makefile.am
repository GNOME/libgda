lib_LTLIBRARIES = libgda_handlers-3.0.la

AM_CPPFLAGS = \
        -I$(top_srcdir) -I$(srcdir)/.. \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS) 

libgda_handlers_headers = \
	gda-handler-bin.h \
	gda-handler-boolean.h \
	gda-handler-numerical.h \
	gda-handler-string.h \
	gda-handler-time.h \
	gda-handler-type.h

libgda_handlersincludedir=$(includedir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/libgda/handlers
libgda_handlersinclude_HEADERS=$(libgda_handlers_headers)

libgda_handlers_3_0_la_SOURCES = \
	$(libgda_handlers_headers) \
	gda-handler-bin.c \
	gda-handler-boolean.c \
	gda-handler-numerical.c \
	gda-handler-string.c \
	gda-handler-time.c \
	gda-handler-type.c

libgda_handlers_3_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) $(NO_UNDEFINED)
libgda_handlers_3_0_la_LIBADD = ../libgda-3.0.la $(LIBGDA_LIBS)
