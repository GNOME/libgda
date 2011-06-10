/* GDA common library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Johannes Schmid
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_CUSTOM_MARSHAL_H__
#define __GDA_CUSTOM_MARSHAL_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* VOID:GTYPE,GTYPE */
void
_gda_marshal_VOID__GTYPE_GTYPE (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data);

/* ERROR:OBJECT,VALUE */
void
_gda_marshal_ERROR__OBJECT_VALUE (GClosure     *closure,
				 GValue       *return_value G_GNUC_UNUSED,
				 guint         n_param_values,
				 const GValue *param_values,
				 gpointer      invocation_hint G_GNUC_UNUSED,
				 gpointer      marshal_data);

/* VOID:OBJECT,STRING,VALUE */
void
_gda_marshal_VOID__OBJECT_STRING_VALUE (GClosure     *closure,
				       GValue       *return_value G_GNUC_UNUSED,
				       guint         n_param_values,
				       const GValue *param_values,
				       gpointer      invocation_hint G_GNUC_UNUSED,
				       gpointer      marshal_data);

/* ERROR:VOID */
void
_gda_marshal_ERROR__VOID (GClosure     *closure,
			 GValue       *return_value G_GNUC_UNUSED,
			 guint         n_param_values,
			 const GValue *param_values,
			 gpointer      invocation_hint G_GNUC_UNUSED,
			 gpointer      marshal_data);


/* VOID:STRING,VALUE */
void
_gda_marshal_VOID__STRING_VALUE (GClosure     *closure,
				GValue       *return_value G_GNUC_UNUSED,
				guint         n_param_values,
				const GValue *param_values,
				gpointer      invocation_hint G_GNUC_UNUSED,
				gpointer      marshal_data);

/* ERROR:VALUE */
void
_gda_marshal_ERROR__VALUE (GClosure     *closure,
			  GValue       *return_value G_GNUC_UNUSED,
			  guint         n_param_values,
			  const GValue *param_values,
			  gpointer      invocation_hint G_GNUC_UNUSED,
			  gpointer      marshal_data);

/* ERROR:INT,INT */
void
_gda_marshal_ERROR__INT_INT (GClosure     *closure,
			    GValue       *return_value G_GNUC_UNUSED,
			    guint         n_param_values,
			    const GValue *param_values,
			    gpointer      invocation_hint G_GNUC_UNUSED,
			    gpointer      marshal_data);

/* VOID:SLIST */
void
_gda_marshal_VOID__SLIST (GClosure     *closure,
			  GValue       *return_value G_GNUC_UNUSED,
			  guint         n_param_values,
			  const GValue *param_values,
			  gpointer      invocation_hint G_GNUC_UNUSED,
			  gpointer      marshal_data);

/* ERROR:GdaMetaContext */
void
_gda_marshal_ERROR__METACONTEXT (GClosure     *closure,
				 GValue       *return_value G_GNUC_UNUSED,
				 guint         n_param_values,
				 const GValue *param_values,
				 gpointer      invocation_hint G_GNUC_UNUSED,
				 gpointer      marshal_data);

G_END_DECLS

#endif
