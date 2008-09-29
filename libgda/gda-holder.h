/* gda-holder.h
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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


#ifndef __GDA_HOLDER_H_
#define __GDA_HOLDER_H_

#include <libgda/gda-decl.h>
#include "gda-value.h"

G_BEGIN_DECLS

#define GDA_TYPE_HOLDER          (gda_holder_get_type())
#define GDA_HOLDER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_holder_get_type(), GdaHolder)
#define GDA_HOLDER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_holder_get_type (), GdaHolderClass)
#define GDA_IS_HOLDER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_holder_get_type ())

/* error reporting */
extern GQuark gda_holder_error_quark (void);
#define GDA_HOLDER_ERROR gda_holder_error_quark ()

typedef enum {
	GDA_HOLDER_STRING_CONVERSION_ERROR,
	GDA_HOLDER_VALUE_TYPE_ERROR,
	GDA_HOLDER_VALUE_NULL_ERROR
} GdaHolderError;

/* struct for the object's data */
struct _GdaHolder
{
	GObject                 object;
	GdaHolderPrivate       *priv;
};


/* struct for the object's class */
struct _GdaHolderClass
{
	GObjectClass               parent_class;
	void                     (*changed)          (GdaHolder *holder);
	void                     (*source_changed)   (GdaHolder *holder);
	GError                  *(*validate_change)  (GdaHolder *holder, const GValue *new_value);
};

GType               gda_holder_get_type                (void) G_GNUC_CONST;
GdaHolder          *gda_holder_new                     (GType type);
GdaHolder          *gda_holder_new_inline              (GType type, const gchar *id, ...);
GdaHolder          *gda_holder_copy                    (GdaHolder *orig);

GType               gda_holder_get_g_type              (GdaHolder *holder);
const gchar        *gda_holder_get_id                  (GdaHolder *holder);

#define gda_holder_new_string(id,str) gda_holder_new_inline (G_TYPE_STRING, (id), (str))
#define gda_holder_new_boolean(id,abool) gda_holder_new_inline (G_TYPE_BOOLEAN, (id), (abool))
#define gda_holder_new_int(id,anint) gda_holder_new_inline (G_TYPE_INT, (id), (anint))

const GValue       *gda_holder_get_value               (GdaHolder *holder);
gchar              *gda_holder_get_value_str           (GdaHolder *holder, GdaDataHandler *dh);
gboolean            gda_holder_set_value               (GdaHolder *holder, const GValue *value, GError **error);
gboolean            gda_holder_take_value              (GdaHolder *holder, GValue *value, GError **error);
GValue             *gda_holder_take_static_value       (GdaHolder *holder, const GValue *value, GError **error);
gboolean            gda_holder_set_value_str           (GdaHolder *holder, GdaDataHandler *dh, const gchar *value, GError **error);

const GValue       *gda_holder_get_default_value       (GdaHolder *holder);
void                gda_holder_set_default_value       (GdaHolder *holder, const GValue *value);
gboolean            gda_holder_set_value_to_default    (GdaHolder *holder);
gboolean            gda_holder_value_is_default        (GdaHolder *holder);

void                gda_holder_force_invalid           (GdaHolder *holder);
gboolean            gda_holder_is_valid                (GdaHolder *holder);


void                gda_holder_set_not_null            (GdaHolder *holder, gboolean not_null);
gboolean            gda_holder_get_not_null            (GdaHolder *holder);

gboolean            gda_holder_set_source_model         (GdaHolder *holder, GdaDataModel *model,
							 gint col, GError **error);
GdaDataModel       *gda_holder_get_source_model         (GdaHolder *holder, gint *col);

gboolean            gda_holder_set_bind                 (GdaHolder *holder, GdaHolder *bind_to, GError **error);
GdaHolder          *gda_holder_get_bind                 (GdaHolder *holder);

G_END_DECLS

#endif
