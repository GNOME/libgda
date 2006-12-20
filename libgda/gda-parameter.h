/* gda-parameter.h
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef __GDA_PARAMETER_H_
#define __GDA_PARAMETER_H_

#include <libgda/gda-object.h>
#include "gda-value.h"

G_BEGIN_DECLS

#define GDA_TYPE_PARAMETER          (gda_parameter_get_type())
#define GDA_PARAMETER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_parameter_get_type(), GdaParameter)
#define GDA_PARAMETER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_parameter_get_type (), GdaParameterClass)
#define GDA_IS_PARAMETER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_parameter_get_type ())

/* error reporting */
extern GQuark gda_parameter_error_quark (void);
#define GDA_PARAMETER_ERROR gda_parameter_error_quark ()

typedef enum {
	GDA_PARAMETER_QUERY_LIMIT_ERROR
} GdaParameterError;

/* struct for the object's data */
struct _GdaParameter
{
	GdaObject                  object;
	GdaParameterPrivate       *priv;
};


/* struct for the object's class */
struct _GdaParameterClass
{
	GdaObjectClass             parent_class;
	void                     (*restrict_changed)         (GdaParameter *param);
};

GType                  gda_parameter_get_type                (void);
GdaParameter          *gda_parameter_new                     (GType type);
GdaParameter          *gda_parameter_new_copy                (GdaParameter *orig);
GType                  gda_parameter_get_g_type              (GdaParameter *param);

GdaParameter          *gda_parameter_new_string              (const gchar *name, const gchar *str);
GdaParameter          *gda_parameter_new_boolean             (const gchar *name, gboolean value);

void                   gda_parameter_declare_param_user      (GdaParameter *param, GdaObject *user);
GSList                *gda_parameter_get_param_users         (GdaParameter *param);
void                   gda_parameter_replace_param_users     (GdaParameter *param, GHashTable *replacements);

const GValue          *gda_parameter_get_value               (GdaParameter *param);
gchar                 *gda_parameter_get_value_str           (GdaParameter *param);
void                   gda_parameter_set_value               (GdaParameter *param, const GValue *value);
gboolean               gda_parameter_set_value_str           (GdaParameter *param, const gchar *value);

void                   gda_parameter_declare_invalid         (GdaParameter *param);
gboolean               gda_parameter_is_valid                (GdaParameter *param);

const GValue          *gda_parameter_get_default_value       (GdaParameter *param);
void                   gda_parameter_set_default_value       (GdaParameter *param, const GValue *value);
gboolean               gda_parameter_get_exists_default_value(GdaParameter *param);
void                   gda_parameter_set_exists_default_value(GdaParameter *param, gboolean default_value_exists);

void                   gda_parameter_set_not_null            (GdaParameter *param, gboolean not_null);
gboolean               gda_parameter_get_not_null            (GdaParameter *param);

gboolean               gda_parameter_restrict_values         (GdaParameter *param, GdaDataModel *model,
							      gint col, GError **error);
gboolean               gda_parameter_has_restrict_values     (GdaParameter *param, GdaDataModel **model,
							      gint *col);
void                   gda_parameter_bind_to_param           (GdaParameter *param, GdaParameter *bind_to);
GdaParameter          *gda_parameter_get_bind_param          (GdaParameter *param);

G_END_DECLS

#endif
