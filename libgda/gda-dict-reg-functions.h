/* gda-dict-reg-functions.h
 *
 * Copyright (C) 2006 Vivien Malerba
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


#ifndef __GDA_DICT_REG_FUNCTIONS_H_
#define __GDA_DICT_REG_FUNCTIONS_H_

#include "gda-dict-private.h"

GdaDictRegisterStruct *gda_functions_get_register ();

GSList                *gda_functions_get_by_name (GdaDict *dict, const gchar *aggname);
GdaDictFunction       *gda_functions_get_by_name_arg (GdaDict *dict, 
						      const gchar *aggname, const GSList *argtypes);
GdaDictFunction       *gda_functions_get_by_name_arg_in_list (GdaDict *dict, GSList *functions, 
							       const gchar *aggname, const GSList *argtypes);
GdaDictFunction       *gda_functions_get_by_dbms_id (GdaDict *dict, const gchar *dbms_id);

#endif
