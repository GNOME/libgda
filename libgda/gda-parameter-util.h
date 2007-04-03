/* gda-parameter-util.h
 *
 * Copyright (C) 2007 Vivien Malerba
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
 */SA
 */


#ifndef __GDA_PARAMETER_UTIL_H_
#define __GDA_PARAMETER_UTIL_H_

#include <glib.h>
#include <libgda/gda-object.h>
#include "gda-decl.h"

G_BEGIN_DECLS

void                   gda_parameter_declare_param_user      (GdaParameter *param, GdaObject *user);
GSList                *gda_parameter_get_param_users         (GdaParameter *param);
void                   gda_parameter_replace_param_users     (GdaParameter *param, GHashTable *replacements);

GdaParameter          *gda_parameter_list_find_param_for_user (GdaParameterList *paramlist, 
							       GdaObject *user);

gchar                 *gda_parameter_get_alphanum_name       (GdaParameter *param);

G_END_DECLS

#endif
