/* GNOME-DB
 * Copyright (c) 2000 by Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __GDA_BUILTIN_RES_H__
#define __GDA_BUILTIN_RES_H__

#include <stdio.h>
#include <glib.h>

typedef struct _Gda_Builtin_Res_AttDesc
{
  char         *name;	          	/* field name */
  gulong       typid;    		/* type id */  
  int          typlen;   		/* type size */
} Gda_Builtin_Res_AttDesc;


/* We use char* for Attribute values.
   The value pointer always points to a null-terminated area; we add a
   null (zero) byte after whatever the backend sends us.  
*/
   
typedef struct _Gda_Builtin_Res_AttValue
{
  int	    len;       /* length in bytes of the value */
  char      *value;    /* actual value, plus terminating zero byte */
} Gda_Builtin_Res_AttValue;


typedef struct _Gda_Builtin_Result
{
  int                      ntuples;
  int                      numcols;
  Gda_Builtin_Res_AttDesc  *attDescs;
  Gda_Builtin_Res_AttValue **tuples;
} Gda_Builtin_Result;

/* returns an empty Gda_Builtin_Result */
Gda_Builtin_Result* Gda_Builtin_Result_new(guint ncols, 
					   gchar *default_name,
					   gulong default_typid, 
					   int default_typlen);

/* frees ALL the Gda_Builtin_Result* */
void                Gda_Builtin_Result_free(Gda_Builtin_Result* res);

/* sets the attributes for one column */
void                Gda_Builtin_Result_set_att(Gda_Builtin_Result* res,
					       guint col,
					       gchar *fieldname,
					       gulong typid, 
					       int typlen);

/* add a new row to the result and returns the row number */
guint               Gda_Builtin_Result_add_row(Gda_Builtin_Result* res,
					       gchar **row);

/* information about the types */
gchar*              Gda_Builtin_Result_get_fname(Gda_Builtin_Result* res,
						 guint col);
gulong              Gda_Builtin_Result_get_ftype(Gda_Builtin_Result* res,
						 guint col);
gint                Gda_Builtin_Result_get_fsize(Gda_Builtin_Result* res,
						 guint col);

guint               Gda_Builtin_Result_get_nbtuples(Gda_Builtin_Result* res);
guint               Gda_Builtin_Result_get_nbfields(Gda_Builtin_Result* res);

/* returns a non allocated string => do not modify or deallocated it */
gchar*              Gda_Builtin_Result_get_value(Gda_Builtin_Result* res,
						 guint row, guint col);

gint                Gda_Builtin_Result_get_length(Gda_Builtin_Result* res,
						  guint row, guint col);


/* debug function */
void                Gda_Builtin_Result_dump(Gda_Builtin_Result* res);
#endif
