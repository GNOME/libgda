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

typedef struct _GdaBuiltin_Res_AttDesc
{
	char *name;		/* field name */
	gulong typid;		/* type id */
	int typlen;		/* type size */
}
GdaBuiltin_Res_AttDesc;


/* We use char* for Attribute values.
   The value pointer always points to a null-terminated area; we add a
   null (zero) byte after whatever the backend sends us.  
*/

typedef struct _GdaBuiltin_Res_AttValue
{
	int len;		/* length in bytes of the value */
	char *value;		/* actual value, plus terminating zero byte */
}
GdaBuiltin_Res_AttValue;


typedef struct _GdaBuiltin_Result
{
	int ntuples;
	int numcols;
	GdaBuiltin_Res_AttDesc *attDescs;
	GdaBuiltin_Res_AttValue **tuples;

	gulong default_typid;
}
GdaBuiltin_Result;

/* returns an empty GdaBuiltin_Result */
GdaBuiltin_Result *GdaBuiltin_Result_new (guint ncols,
					  gchar * default_name,
					  gulong default_typid,
					  int default_typlen);

/* frees ALL the GdaBuiltin_Result* */
void GdaBuiltin_Result_free (GdaBuiltin_Result * res);

/* sets the attributes for one column */
void GdaBuiltin_Result_set_att (GdaBuiltin_Result * res,
				guint col,
				gchar * fieldname, gulong typid, int typlen);

/* add a new row to the result and returns the row number */
guint GdaBuiltin_Result_add_row (GdaBuiltin_Result * res, gchar ** row);

/* information about the types */
gchar *GdaBuiltin_Result_get_fname (GdaBuiltin_Result * res, guint col);
gulong GdaBuiltin_Result_get_ftype (GdaBuiltin_Result * res, guint col);
gint GdaBuiltin_Result_get_fsize (GdaBuiltin_Result * res, guint col);

guint GdaBuiltin_Result_get_nbtuples (GdaBuiltin_Result * res);
guint GdaBuiltin_Result_get_nbfields (GdaBuiltin_Result * res);

/* returns a non allocated string => do not modify or deallocated it */
gchar *GdaBuiltin_Result_get_value (GdaBuiltin_Result * res,
				    guint row, guint col);

gint GdaBuiltin_Result_get_length (GdaBuiltin_Result * res,
				   guint row, guint col);


/* debug function */
void GdaBuiltin_Result_dump (GdaBuiltin_Result * res);
#endif
