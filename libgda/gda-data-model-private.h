/* GDA common library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDA_DATA_MODEL_PRIVATE_H__
#define __GDA_DATA_MODEL_PRIVATE_H__

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-command.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-column.h>
#include <libgda/gda-value.h>
#include <libgda/gda-data-model-extra.h>

G_BEGIN_DECLS

const gchar                  *gda_data_model_get_command_text       (GdaDataModel *model);
gboolean                      gda_data_model_set_command_text       (GdaDataModel *model, const gchar *txt);
GdaCommandType                gda_data_model_get_command_type       (GdaDataModel *model);
gboolean                      gda_data_model_set_command_type       (GdaDataModel *model, GdaCommandType type);

xmlNodePtr                    gda_data_model_to_xml_node            (GdaDataModel *model, const gint *cols, gint nb_cols, 
								     const gint *rows, gint nb_rows, const gchar *name);
gboolean                      gda_data_model_add_data_from_xml_node (GdaDataModel *model, xmlNodePtr node, GError **error);

G_END_DECLS

#endif
