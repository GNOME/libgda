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

#include <gda-builtin-res.h>

Gda_Builtin_Result* Gda_Builtin_Result_new(guint ncols,
					   gchar *default_name,
					   gulong default_typid, 
					   int default_typlen)
{
  Gda_Builtin_Result *res;
  Gda_Builtin_Res_AttDesc *attdesc;
  guint i;

  res = g_new(Gda_Builtin_Result, 1);
  res->ntuples = 0;
  res->numcols = ncols;
  res->attDescs = g_new(Gda_Builtin_Res_AttDesc, ncols);
  for (i=0; i<ncols; i++)
    {
      attdesc = res->attDescs+i;
      attdesc->name = g_strdup(default_name);
      attdesc->typid = default_typid;
      attdesc->typlen = default_typlen;
    }
  res->tuples = NULL;

  return res;
}

void Gda_Builtin_Result_free(Gda_Builtin_Result* res)
{
  gint i, j;
  Gda_Builtin_Res_AttValue *val;

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

void Gda_Builtin_Result_set_att(Gda_Builtin_Result* res,
				guint col,
				gchar *fieldname,
				gulong typid, 
				int typlen)
{
  Gda_Builtin_Res_AttDesc *attdesc;
  
  attdesc = res->attDescs+col;
  if (attdesc->name)
    g_free(attdesc->name);
  attdesc->name = g_strdup(fieldname);
  attdesc->typid = typid;
  attdesc->typlen = typlen;
}

guint Gda_Builtin_Result_add_row(Gda_Builtin_Result* res,
				 gchar **row)
{
  Gda_Builtin_Res_AttValue *newval;
  guint i;

  res->ntuples ++;
  if (res->tuples)
    res->tuples = g_renew(Gda_Builtin_Res_AttValue *, res->tuples, 
			  res->ntuples);
  else
    res->tuples = g_new(Gda_Builtin_Res_AttValue *, res->ntuples);

  /* memory for that tuple */
  *(res->tuples+(res->ntuples-1)) = 
    g_new(Gda_Builtin_Res_AttValue, res->numcols);
  /* filling the new tuple */
  /*fprintf(stderr, "New values at %p\n", *(res->tuples+(res->ntuples-1)));*/
  for (i=0; i<res->numcols; i++)
    {
      newval = (*(res->tuples+(res->ntuples-1))) + i;
      /*fprintf(stderr, "   New val at %p\n", newval);*/
      newval->len = strlen(*(row+i)) + 1; /* for the final '\0' */
      newval->value = g_strdup(*(row+i));
    }

  return res->ntuples-1; /* rows start at 0 */
}

guint Gda_Builtin_Result_get_nbtuples(Gda_Builtin_Result* res)
{
  if (res)
    return res->ntuples;
  else
    return 0;
}

guint Gda_Builtin_Result_get_nbfields(Gda_Builtin_Result* res)
{
  if (res)
    return res->numcols;
  else
    return 0;
}

gchar* Gda_Builtin_Result_get_fname(Gda_Builtin_Result* res,
				    guint col)
{
  if (!res)
    return NULL;
  if (col > res->numcols-1) /* out of range */
    return NULL;

  return (res->attDescs+col)->name;
}

gulong Gda_Builtin_Result_get_ftype(Gda_Builtin_Result* res,
				    guint col)
{
  if (!res)
    return 0;
  if (col > res->numcols-1) /* out of range */
    return 0;

  return (res->attDescs+col)->typid;
}

gint Gda_Builtin_Result_get_fsize(Gda_Builtin_Result* res,
				  guint col)
{
  if (!res)
    return -1;
  if (col > res->numcols-1) /* out of range */
    return -1;

  return (res->attDescs+col)->typlen;
}

gint Gda_Builtin_Result_get_length(Gda_Builtin_Result* res,
				   guint row, guint col)
{
  if (!res)
    return -1;
  if ((row > res->ntuples-1) || (col > res->numcols-1)) /* out of range */
    return -1;

  return ((*(res->tuples+row)) + col)->len;
}

gchar* Gda_Builtin_Result_get_value(Gda_Builtin_Result* res,
				    guint row, guint col)
{
  if (!res)
    return NULL;
  if ((row > res->ntuples-1) || (col > res->numcols-1)) /* out of range */
    return NULL;

  return ((*(res->tuples+row)) + col)->value;
}

void Gda_Builtin_Result_dump(Gda_Builtin_Result* res)
{
  gint i, j;
  Gda_Builtin_Res_AttValue *val;

  if (!res)
    {
      fprintf(stderr, "Gda_Builtin_Result_dump: Result is NULL\n");
      return;
    }

  if (res->ntuples == 0)
    {
      fprintf(stderr, "Gda_Builtin_Result_dump: Result is empty\n");
      return;
    }

  fprintf(stderr, "Gda_Builtin_Result_dump: Result has %d tuples\n",
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
