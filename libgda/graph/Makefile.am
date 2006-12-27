lib_LTLIBRARIES = libgda_graph-3.0.la

AM_CPPFLAGS = \
        -I$(top_srcdir) -I$(srcdir)/.. \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS) 

libgda_graph_headers = \
	gda-graph.h \
	gda-graph-item.h \
	gda-graph-query.h \
	gda-dict-reg-graphs.h

libgda_graphincludedir=$(includedir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/libgda/graph
libgda_graphinclude_HEADERS=$(libgda_graph_headers)

libgda_graph_3_0_la_SOURCES = \
	$(libgda_graph_headers) \
	gda-graph.c \
        gda-graph-item.c \
        gda-graph-query.c \
	gda-dict-reg-graphs.c

libgda_graph_3_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) $(NO_UNDEFINED)
libgda_graph_3_0_la_LIBADD = ../libgda-3.0.la $(LIBGDA_LIBS)
