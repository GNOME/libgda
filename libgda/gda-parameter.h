/* GDA library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_parameter_h__)
#  define __gda_parameter_h__

#include <glib/gmacros.h>
#include <libgda/gda-value.h>

G_BEGIN_DECLS

typedef GNOME_Database_Parameter GdaParameter;

GdaParameter *gda_parameter_new (const gchar *name, GdaValueType type);
void          gda_parameter_free (GdaParameter *param);
const gchar  *gda_parameter_get_name (GdaParameter *param);
void          gda_parameter_set_name (GdaParameter *param, const gchar *name);
GdaValue     *gda_parameter_get_value (GdaParameter *param);
void          gda_parameter_set_value (GdaParameter *param, GdaValue *value);

typedef struct _GdaParameterList GdaParameterList;

GdaParameterList             *gda_parameter_list_new (void);
void                          gda_parameter_list_free (GdaParameterList *plist);
void                          gda_parameter_list_add_parameter (GdaParameterList *plist,
								GdaParameter *param);
GdaParameter                 *gda_parameter_list_find (GdaParameterList *plist,
						       const gchar *name);
void                          gda_parameter_list_clear (GdaParameterList *plist);
guint                         gda_parameter_list_get_length (GdaParameterList *plist);

GNOME_Database_ParameterList *gda_parameter_list_to_corba (GdaParameterList *plist);

G_END_DECLS

#endif
