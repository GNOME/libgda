/*
 * Copyright (C) 2006 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_SQLITE_HANDLER_BIN__
#define __GDA_SQLITE_HANDLER_BIN__

#include <glib-object.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_SQLITE_HANDLER_BIN          (gda_sqlite_handler_bin_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaSqliteHandlerBin, gda_sqlite_handler_bin, GDA, SQLITE_HANDLER_BIN, GObject)

/* struct for the object's class */
struct _GdaSqliteHandlerBinClass
{
	GObjectClass              parent_class;
};


GdaDataHandler *_gda_sqlite_handler_bin_new           (void);

G_END_DECLS

#endif
