noinst_LTLIBRARIES = libbasetool.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	-I$(top_builddir)/libgda \
        $(COREDEPS_CFLAGS) \
        $(COREDEPS_WFLAGS)


libbasetool_la_SOURCES = \
	base-tool.h \
	base-tool-decl.h \
	base-tool-defines.h \
        base-tool-errors.h \
        base-tool-errors.c \
        base-tool-input.h \
        base-tool-input.c \
        base-tool-output.h \
        base-tool-output.c \
        base-tool-command.h \
        base-tool-command.c \
	base-tool-help.h \
	base-tool-help.c
