# GDA Oracle provider
# Copyright (C) 1998 - 2007 The GNOME Foundation
#
# AUTHORS:
#	Rodrigo Moya <rodrigo@gnome-db.org>
#	Vivien Malerba <malerba@gnome-db.org>
#	Bas Driessen <bas.driessen@xobas.com>
#
# This Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this Library; see the file COPYING.LIB.  If not,
# write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-oracle.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(ORACLE_CFLAGS)

libgda_oracle_la_SOURCES = \
	gda-oracle-blob-op.c \
	gda-oracle-blob-op.h \
	gda-oracle-ddl.c \
	gda-oracle-ddl.h \
	gda-oracle-provider.c \
	gda-oracle-provider.h \
	gda-oracle-recordset.c \
	gda-oracle-recordset.h \
	gda-oracle.h \
	libmain.c \
	utils.c
libgda_oracle_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_oracle_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) \
	$(ORACLE_LIBS)

xmldir   = $(datadir)/libgda-4.0
xml_in_files =	oracle_specs_dsn.xml.in \
		oracle_specs_drop_table.xml.in \
		oracle_specs_drop_index.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-oracle-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-oracle-4.0.pc.in
DISTCLEANFILES = $(xml_DATA)
