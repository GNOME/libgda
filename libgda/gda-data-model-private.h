/*
 * Copyright (C) 2005 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2006 - 2007 Murray Cumming <murrayc@murrayc-desktop>
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

#ifndef __GDA_DATA_MODEL_PRIVATE_H__
#define __GDA_DATA_MODEL_PRIVATE_H__

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-column.h>
#include <libgda/gda-value.h>
#include <libgda/gda-data-model-extra.h>

G_BEGIN_DECLS

gboolean                      gda_data_model_add_data_from_xml_node (GdaDataModel *model, xmlNodePtr node, GError **error);

G_END_DECLS

#endif
