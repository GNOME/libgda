/* GNOME-DB
 * Copyright (c) 2000, 2001 by Vivien Malerba
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

#include "gda-builtin-res.h"

GdaBuiltin_Result* GdaBuiltin_Result_new(guint ncols,
					   gchar *default_name,
					   gulong default_typid, 
					   int default_typlen)
{
  GdaBuiltin_Result *res;
  GdaBuiltin_Res_AttDesc *attdesc;
  guint i;

  res = g_new(GdaBuiltin_Result, 1);
  res->ntuples = 0;
  res->numcols = ncols;
  res->attDescs = g_new(GdaBuiltin_Res_AttDesc, ncols);
  for (i=0; i<ncols; i++)
    {
      attdesc = res->attDescs+i;
      attdesc->name = g_strdup(default_name);
      attdesc->typid = default_typid;
      attdesc->typlen = default_typlen;
    }
  res->tuples = NULL;

  res->default_typid = default_typid;
  return res;
}

void GdaBuiltin_Result_free(GdaBuiltin_Result* res)
{
  gint i, j;
  GdaBuiltin_Res_AttValue *val;

  /* attributes */
  if (res->attDescs)
    {
      for (i=0; i<res->numcols; i++)
	{
	  if ((res->attDescs+i)->name)
	    g_free((res->attDescs+i)->name);
	}
      g_free(res->attDescs);
    }

  /* values */
  if (res->tuples)
    {
      for (i=0; i<res->ntuples; i++)
	{
	  /*fprintf(stderr, "Tuple at %p being freed\n", *(res->tuples+i));*/
	  for (j=0; j<res->numcols; j++)
	    {
	      val = (*(res->tuples+i)) + j;
	      /*fprintf(stderr, "   Val at %p\n", val);*/
	      g_free(val->value);
	    }
	  g_free(*(res->tuples+i));
	}
      g_free(res->tuples);
    }
}

void GdaBuiltin_Result_set_att(GdaBuiltin_Result* res,
				guint col,
				gchar *fieldname,
				gulong typid, 
				int typlen)
{
  GdaBuiltin_Res_AttDesc *attdesc;
  
  attdesc = res->attDescs+col;
  if (attdesc->name)
    g_free(attdesc->name);
  attdesc->name = g_strdup(fieldname);
  attdesc->typid = typid;
  attdesc->typlen = typlen;
}

guint GdaBuiltin_Result_add_row(GdaBuiltin_Result* res,
				 gchar **row)
{
  GdaBuiltin_Res_AttValue *newval;
  guint i;

  res->ntuples ++;
  if (res->tuples)
    res->tuples = g_renew(GdaBuiltin_Res_AttValue *, res->tuples, 
			  res->ntuples);
  else
    res->tuples = g_new(GdaBuiltin_Res_AttValue *, res->ntuples);

  /* memory for that tuple */
  *(res->tuples+(res->ntuples-1)) = 
    g_new(GdaBuiltin_Res_AttValue, res->numcols);
  /* filling the new tuple */
  /*fprintf(stderr, "New values at %p\n", *(res->tuples+(res->ntuples-1)));*/
  for (i=0; i<res->numcols; i++)
    {
      newval = (*(res->tuples+(res->ntuples-1))) + i;
      /*fprintf(stderr, "   New val at %p\n", newval);*/
      if (*(row+i)) {
	newval->len = strlen(*(row+i)) + 1; /* for the final '\0' */
	newval->value = g_strdup(*(row+i));
      }
      else {
	newval->len = 0;
	newval->value = NULL;
      }
    }

  return res->ntuples-1; /* rows start at 0 */
}

guint GdaBuiltin_Result_get_nbtuples(GdaBuiltin_Result* res)
{
  if (res)
    return res->ntuples;
  else
    return 0;
}

guint GdaBuiltin_Result_get_nbfields(GdaBuiltin_Result* res)
{
  if (res)
    return res->numcols;
  else
    return 0;
}

gchar* GdaBuiltin_Result_get_fname(GdaBuiltin_Result* res,
				    guint col)
{
  if (!res)
    return NULL;
  if (col > res->numcols-1) /* out of range */
    return NULL;

  return (res->attDescs+col)->name;
}

gulong GdaBuiltin_Result_get_ftype(GdaBuiltin_Result* res,
				    guint col)
{
  if (!res)
    return res->default_typid;
  if (col > res->numcols-1) /* out of range */
    return res->default_typid;

  return (res->attDescs+col)->typid;
}

gint GdaBuiltin_Result_get_fsize(GdaBuiltin_Result* res,
				  guint col)
{
  if (!res)
    return -1;
  if (col > res->numcols-1) /* out of range */
    return -1;

  return (res->attDescs+col)->typlen;
}

gint GdaBuiltin_Result_get_length(GdaBuiltin_Result* res,
				   guint row, guint col)
{
  if (!res)
    return -1;
  if ((row > res->ntuples-1) || (col > res->numcols-1)) /* out of range */
    return -1;

  return ((*(res->tuples+row)) + col)->len;
}

gchar* GdaBuiltin_Result_get_value(GdaBuiltin_Result* res,
				    guint row, guint col)
{
  if (!res)
    return NULL;
  if ((row > res->ntuples-1) || (col > res->numcols-1)) /* out of range */
    return NULL;

  return ((*(res->tuples+row)) + col)->value;
}

void GdaBuiltin_Result_dump(GdaBuiltin_Result* res)
{
  gint i, j;
  GdaBuiltin_Res_AttValue *val;

  if (!res)
    {
      fprintf(stderr, "GdaBuiltin_Result_dump: Result is NULL\n");
      return;
    }

  if (res->ntuples == 0)
    {
      fprintf(stderr, "GdaBuiltin_Result_dump: Result is empty\n");
      return;
    }

  fprintf(stderr, "GdaBuiltin_Result_dump: Result has %d tuples\n",
	  res->ntuples);
  for (i=0; i<res->ntuples; i++)
    {
      fprintf(stderr, "--------------Row: %03d at %p\n", i,
	      *(res->tuples+i));
      for (j=0; j<res->numcols; j++)
	{
	  val = (*(res->tuples+i)) + j;
	  fprintf(stderr, "  %s at %p = %s\n", (res->attDescs+j)->name, 
		  val, val->value);
	}
    }
}
