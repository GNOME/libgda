/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
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


#ifndef __GDA_BATCH_H_
#define __GDA_BATCH_H_

#include <glib-object.h>
#include <libgda/gda-statement.h>

G_BEGIN_DECLS

#define GDA_TYPE_BATCH          (gda_batch_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaBatch, gda_batch, GDA, BATCH, GObject)

/* error reporting */
extern GQuark gda_batch_error_quark (void);
#define GDA_BATCH_ERROR gda_batch_error_quark ()

typedef enum {
	GDA_BATCH_CONFLICTING_PARAMETER_ERROR
} GdaBatchError;

/* struct for the object's class */
struct _GdaBatchClass
{
	GObjectClass     parent_class;

	/* signals */
	void   (*changed) (GdaBatch *batch, GdaStatement *changed_stmt);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-batch
 * @short_description: Multiple SQL statements grouped together.
 * @title: GdaBatch
 * @stability: Stable
 * @see_also: #GdaStatement
 *
 * The #GdaBatch object represents one or more SQL statements (as #GdaStatement objects) in a single object.
 *
 * A #GdaBatch can either be built "manually" by assembling together several #GdaStatement objects,
 * or from an SQL string using a #GdaSqlParser object.
 */

GdaBatch          *gda_batch_new                    (void);
GdaBatch          *gda_batch_copy                   (GdaBatch *orig);
void               gda_batch_add_statement          (GdaBatch *batch, GdaStatement *stmt);
void               gda_batch_remove_statement       (GdaBatch *batch, GdaStatement *stmt);

gchar             *gda_batch_serialize              (GdaBatch *batch);
const GSList      *gda_batch_get_statements         (GdaBatch *batch);
gboolean           gda_batch_get_parameters         (GdaBatch *batch, GdaSet **out_params, GError **error);

G_END_DECLS

#endif
