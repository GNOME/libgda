/*
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2003 Xabier Rodríguez Calvar <xrcalvar@igalia.com>
 * Copyright (C) 2004 Paisa  Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2005 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 Álvaro Peńa <alvaropg@telefonica.net>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_ROW_H__
#define __GDA_ROW_H__

#include <glib-object.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_ROW            (gda_row_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaRow, gda_row, GDA, ROW, GObject)
struct _GdaRowClass {
	GObjectClass   parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-row
 * @short_description: Individual row of a #GdaDataModelArray object
 * @title: GdaRow
 * @stability: Stable
 * @see_also: #GdaDataModelArray
 *
 * The #GdaDataModelArray object uses #GdaRow to store each row of data. Each #GdaRow has the same
 * number of #GValue values (equal to the number of columns of the data model).
 *
 * As a side note, the #GdaRow object is also used internally by the implementation of the data models returned
 * when executing a SELECT statement.
 */

GdaRow       *gda_row_new            (gint count);
GdaRow       *gda_row_new_from_data_model (GdaDataModel *model, guint row);
gint          gda_row_get_length     (GdaRow *row);
GValue       *gda_row_get_value      (GdaRow *row, gint num);

/* for database providers mainly */
void          gda_row_invalidate_value (GdaRow *row, GValue *value);
void          gda_row_invalidate_value_e (GdaRow *row, GValue *value, GError *error);
gboolean      gda_row_value_is_valid (GdaRow *row, GValue *value);
gboolean      gda_row_value_is_valid_e (GdaRow *row, GValue *value, GError **error);

G_END_DECLS

#endif
