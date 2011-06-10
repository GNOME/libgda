/*
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2010 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2005 Cygwin Ports Maintainer <yselkowitz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __LIBGDA_GLOBAL_VARIABLES_H__
#define __LIBGDA_GLOBAL_VARIABLES_H__

#include <libgda/gda-attributes-manager.h>

#ifdef G_OS_WIN32
#define IMPORT __declspec(dllimport)
#else
#define IMPORT
#endif

IMPORT extern gchar *gda_numeric_locale;
IMPORT extern gchar *gda_lang_locale;
IMPORT extern xmlDtdPtr gda_paramlist_dtd;
IMPORT extern GdaAttributesManager *gda_holder_attributes_manager;

#endif
