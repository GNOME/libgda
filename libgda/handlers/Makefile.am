noinst_LTLIBRARIES = libgda_handlers-5.0.la

AM_CPPFLAGS = \
        -I$(top_srcdir) -I$(srcdir)/.. \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS) \
	$(LIBGDA_WFLAGS)

libgda_handlers_headers = \
	gda-handler-bin.h \
	gda-handler-boolean.h \
	gda-handler-numerical.h \
	gda-handler-string.h \
	gda-handler-time.h \
	gda-handler-type.h

libgda_handlersincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/handlers
libgda_handlersinclude_HEADERS=$(libgda_handlers_headers)

libgda_handlers_5_0_la_SOURCES = \
	$(libgda_handlers_headers) \
	gda-handler-bin.c \
	gda-handler-boolean.c \
	gda-handler-numerical.c \
	gda-handler-string.c \
	gda-handler-time.c \
	gda-handler-type.c
