lib_LTLIBRARIES = libgda-clientcpp.la

INCLUDES = \
	-I$(includedir) \
	-I$(top_srcdir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_srcdir)/lib/gda-client \
	-I$(top_builddir)/lib/gda-client \
	$(GDA_CLIENT_CFLAGS)

source_headers = \
	gdaBatch.h \
	gdaCommand.h \
	gdaConnection.h \
	gdaError.h \
	gdaErrorList.h \
	gdaField.h \
	gdaIncludes.h \
	gdaRecordset.h \
	gdaValue.h

gdaincludedir=$(includedir)/libgda-$(VERSION)/gda++
gdainclude_HEADERS=$(source_headers)

libgda_clientcpp_la_SOURCES = \
	$(source_headers) \
	gdaBatch.cpp \
	gdaCommand.cpp \
	gdaConnection.cpp \
	gdaError.cpp \
	gdaErrorList.cpp \
	gdaField.cpp \
	gdaRecordset.cpp \
	gdaValue.cpp

libgda_clientcpp_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE)
libgda_clientcpp_la_LIBADD = \
	$(GDA_CLIENT_LIBS)

