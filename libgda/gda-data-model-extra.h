/*
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DATA_MODEL_EXTRA_H__
#define __GDA_DATA_MODEL_EXTRA_H__

#include <glib-object.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

void     gda_data_model_row_inserted        (GdaDataModel *model, gint row);
void     gda_data_model_row_updated         (GdaDataModel *model, gint row);
void     gda_data_model_row_removed         (GdaDataModel *model, gint row);
void     gda_data_model_reset               (GdaDataModel *model);

G_END_DECLS

#endif
