/* GDA common library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Juan-Mariano de Goyeneche <jmseyas@dit.upm.es> (BLOB issues)
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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <glib/gfileutils.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <glib/gprintf.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda/gda-column.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <libgda/sql-parser/gda-sql-statement.h>

#include <libgda/binreloc/gda-binreloc.h>

/**
 * gda_g_type_to_string
 * @type: Type to convert from.
 *
 * Returns: the string representing the given #GType.
 */
const gchar *
gda_g_type_to_string (GType type)
{
	if (type == GDA_TYPE_NULL)
		return "null";
	else
		return g_type_name (type);
}

/**
 * gda_g_type_from_string
 * @str: the name of a #GType, as returned by gda_g_type_to_string().
 *
 * Returns: the #GType represented by the given @str.
 */
GType
gda_g_type_from_string (const gchar *str)
{
	GType type;
	g_return_val_if_fail (str != NULL, G_TYPE_INVALID);
	
	type = g_type_from_name (str);
	if (type == 0) {
		if (!g_ascii_strcasecmp (str, "int"))
			type = G_TYPE_INT;
		else if (!g_ascii_strcasecmp (str, "string"))
			type = G_TYPE_STRING;
		else if (!g_ascii_strcasecmp (str, "date"))
			type = G_TYPE_DATE;
		else if (!strcmp (str, "boolean"))
                        type = G_TYPE_BOOLEAN;
		else if (!strcmp (str, "null"))
			type = GDA_TYPE_NULL;
		else
			/* could not find a valid GType for @str */
			type = G_TYPE_INVALID;
	}

	return type;
}

/**
 * gda_default_escape_string
 * @string: string to escape
 *
 * Escapes @string to make it understandable by a DBMS. The escape method is very common and replaces any
 * occurence of "'" with "''" and "\" with "\\"
 */
gchar *
gda_default_escape_string (const gchar *string)
{
	gchar *ptr, *ret, *retptr;
	gint size;

	if (!string)
		return NULL;
	
	/* determination of the new string size */
	ptr = (gchar *) string;
	size = 1;
	while (*ptr) {
		if ((*ptr == '\'') ||(*ptr == '\\'))
			size += 2;
		else
			size += 1;
		ptr++;
	}

	ptr = (gchar *) string;
	ret = g_new0 (gchar, size);
	retptr = ret;
	while (*ptr) {
		if (*ptr == '\'') {
			*retptr = '\'';
			*(retptr+1) = *ptr;
			retptr += 2;
		}
		else if (*ptr == '\\') {
			*retptr = '\\';
			*(retptr+1) = *ptr;
			retptr += 2;
		}
		else {
			*retptr = *ptr;
			retptr ++;
		}
		ptr++;
	}
	*retptr = '\0';

	return ret;
}

/**
 * gda_default_unescape_string
 * @string: string to unescape
 *
 * Do the reverse of gda_default_escape_string(): transforms any "\'" into "'" and any
 * "\\" into "\". 
 *
 * Returns: a new unescaped string, or %NULL in an error was found in @string
 */
gchar *
gda_default_unescape_string (const gchar *string)
{
	glong total;
	gchar *ptr;
	gchar *retval;
	glong offset = 0;
	
	if (!string) 
		return NULL;
	
	total = strlen (string);
	retval = g_memdup (string, total+1);
	ptr = (gchar *) retval;
	while (offset < total) {
		/* we accept the "''" as a synonym of "\'" */
		if (*ptr == '\'') {
			if (*(ptr+1) == '\'') {
				g_memmove (ptr+1, ptr+2, total - offset);
				offset += 2;
			}
			else {
				g_free (retval);
				return NULL;
			}
		}
		if (*ptr == '\\') {
			if (*(ptr+1) == '\\') {
				g_memmove (ptr+1, ptr+2, total - offset);
				offset += 2;
			}
			else {
				if (*(ptr+1) == '\'') {
					*ptr = '\'';
					g_memmove (ptr+1, ptr+2, total - offset);
					offset += 2;
				}
				else {
					g_free (retval);
					return NULL;
				}
			}
		}
		else
			offset ++;

		ptr++;
	}

	return retval;	
}

/**
 * gda_utility_check_data_model
 * @model: a #GdaDataModel object
 * @nbcols: the minimum requested number of columns
 * @Varargs: @nbcols arguments of type GType or -1 (if any data type is accepted)
 *
 * Check the column types of a GdaDataModel.
 *
 * Returns: TRUE if the data model's columns match the provided data types and number
 */
gboolean
gda_utility_check_data_model (GdaDataModel *model, gint nbcols, ...)
{
	gboolean retval = TRUE;
	gint i;

	g_return_val_if_fail (model && GDA_IS_DATA_MODEL (model), FALSE);
	
	/* number of columns */
	if (gda_data_model_get_n_columns (model) < nbcols)
		return FALSE;

	/* type of each column */
	if (nbcols > 0) {
		GdaColumn *att;
		GType mtype, rtype;
		gint argtype;
		va_list ap;

		va_start  (ap, nbcols);
		i = 0;
		while ((i<nbcols) && retval) {
			att = gda_data_model_describe_column (model, i);
			mtype = gda_column_get_g_type (att);
			
			argtype = va_arg (ap, GType);
			if (argtype >= 0) {
				rtype = (GType) argtype;
				if (mtype != rtype) {
					retval = FALSE;
#ifdef GDA_DEBUG
					g_print ("Column %d: Expected %s, got %s\n",
						 i, gda_g_type_to_string (rtype), gda_g_type_to_string (mtype));
#endif
				}
			}
			
			i++;
		}
		va_end (ap);
	}

	return retval;

}

/**
 * gda_utility_data_model_dump_data_to_xml
 * @model: 
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @rows: an array containing which rows of @model will be exported, or %NULL for all rows
 * @nb_rows: the number of rows in @rows
 * @name: name to use for the XML resulting table.
 *
 * Dump the data in a #GdaDataModel into a xmlNodePtr (as used in libxml).
 */
void
gda_utility_data_model_dump_data_to_xml (GdaDataModel *model, xmlNodePtr parent, 
					 const gint *cols, gint nb_cols, 
					 const gint *rows, gint nb_rows,
					 gboolean use_col_ids)
{
	gint nrows, i;
	gint *rcols, rnb_cols;
	gchar **col_ids = NULL;

	/* compute columns if not provided */
	if (!cols) {
		rnb_cols = gda_data_model_get_n_columns (model);
		rcols = g_new (gint, rnb_cols);
		for (i = 0; i < rnb_cols; i++)
			rcols [i] = i;
	}
	else {
		rcols = (gint *) cols;
		rnb_cols = nb_cols;
	}

	if (use_col_ids) {
		gint c;
		col_ids = g_new0 (gchar *, rnb_cols);
		for (c = 0; c < rnb_cols; c++) {
			GdaColumn *column;
			gchar *id;
		
			column = gda_data_model_describe_column (model, rcols [c]);
			g_object_get (G_OBJECT (column), "id", &id, NULL);

			if (id && *id)
				col_ids [c] = g_strdup (id);
			else
				col_ids [c] = g_strdup_printf ("_%d", c);

			g_free(id);
		}
	}

	/* add the model data to the XML output */
	if (!rows)
		nrows = gda_data_model_get_n_rows (model);
	else
		nrows = nb_rows;
	if (nrows > 0) {
		xmlNodePtr row, data, field;
		gint r, c;

		data = xmlNewChild (parent, NULL, (xmlChar*)"gda_array_data", NULL);
		for (r = 0; r < nrows; r++) {
			row = xmlNewChild (data, NULL,  (xmlChar*)"gda_array_row", NULL);
			for (c = 0; c < rnb_cols; c++) {
				GValue *value;
				gchar *str = NULL;
				gboolean isnull = FALSE;

				value = (GValue *) gda_data_model_get_value_at (model, rcols [c], rows ? rows [r] : r);
				if (!value || gda_value_is_null ((GValue *) value)) 
					isnull = TRUE;
				else {
					if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
						str = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
					else if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
						if (!g_value_get_string (value))
							isnull = TRUE;
						else
							str = gda_value_stringify (value);	
					}
					else
						str = gda_value_stringify (value);
				}
				if (!use_col_ids) {
					if (! str || (str && (*str == 0)))
						field = xmlNewChild (row, NULL,  (xmlChar*)"gda_value", NULL);
					else
						field = xmlNewChild (row, NULL,  (xmlChar*)"gda_value", (xmlChar*)str);
				}
				else {
					field = xmlNewChild (row, NULL,  (xmlChar*)"gda_array_value", (xmlChar*)str);
					xmlSetProp(field, (xmlChar*)"colid",  (xmlChar*)col_ids [c]);
				}
				if (isnull)
					xmlSetProp(field,  (xmlChar*)"isnull", (xmlChar*)"t");

				g_free (str);
			}
		}
	}

	if (use_col_ids) {
		gint c;
		for (c = 0; c < rnb_cols; c++) 
			g_free (col_ids [c]);
		g_free (col_ids);
	}
}

/**
 * gda_utility_holder_load_attributes
 * @holder:
 * @node: an xmlNodePtr with a &lt;parameter&gt; tag
 * @sources: a list of #GdaDataModel
 *
 * WARNING: may set the "source" custom string property 
 */
void
gda_utility_holder_load_attributes (GdaHolder *holder, xmlNodePtr node, GSList *sources)
{
	xmlChar *str;
	xmlNodePtr vnode;

	/* set properties from the XML spec */
	str = xmlGetProp (node, BAD_CAST "id");
	if (str) {
		g_object_set (G_OBJECT (holder), "id", (gchar*)str, NULL);
		xmlFree (str);
	}	

	str = xmlGetProp (node, BAD_CAST "name");
	if (str) {
		g_object_set (G_OBJECT (holder), "name", (gchar*)str, NULL);
		xmlFree (str);
	}
	str = xmlGetProp (node, BAD_CAST "descr");
	if (str) {
		g_object_set (G_OBJECT (holder), "description", (gchar*)str, NULL);
		xmlFree (str);
	}
	str = xmlGetProp (node, BAD_CAST "nullok");
	if (str) {
		gda_holder_set_not_null (holder, (*str == 'T') || (*str == 't') ? FALSE : TRUE);
		xmlFree (str);
	}
	else
		gda_holder_set_not_null (holder, FALSE);
	str = xmlGetProp (node, BAD_CAST "plugin");
	if (str) 
		g_object_set_data_full (G_OBJECT (holder), "__gda_entry_plugin", str, xmlFree);
	
	str = xmlGetProp (node, BAD_CAST "source");
	if (str) 
		g_object_set_data_full (G_OBJECT (holder), "source", (gchar*)str, g_free);

	/* set restricting source if specified */
	if (str && sources) {
		gchar *ptr1, *ptr2 = NULL, *tok;
		gchar *source;
			
		source = g_strdup ((gchar*)str);
		ptr1 = strtok_r (source, ":", &tok);
		if (ptr1)
			ptr2 = strtok_r (NULL, ":", &tok);
		
		if (ptr1 && ptr2) {
			GSList *tmp = sources;
			GdaDataModel *model = NULL;
			while (tmp && !model) {
				gchar *mname = g_object_get_data (G_OBJECT (tmp->data), "name");
				if (mname && !strcmp (mname, ptr1))
					model = GDA_DATA_MODEL (tmp->data);
				tmp = g_slist_next (tmp);
			}
			
			if (model) {
				gint fno;
				
				fno = atoi (ptr2);
				if ((fno < 0) ||
				    (fno >= gda_data_model_get_n_columns (model))) 
					g_warning (_("Field number %d not found in source named '%s'"), fno, ptr1); 
				else {
					if (gda_holder_set_source_model (holder, model, fno, NULL)) {
						gchar *str;
						/* rename the wrapper with the current holder's name */
						g_object_get (G_OBJECT (holder), "name", &str, NULL);
						g_object_set_data_full (G_OBJECT (model), "newname", str, g_free);
						g_object_get (G_OBJECT (holder), "description", &str, NULL);
						g_object_set_data_full (G_OBJECT (model), "newdescr", str, g_free);
					}
				}
			}
		}
	}

	/* specified value */
	vnode = node->children;
	if (vnode) {
		xmlChar *this_lang, *isnull;
		const gchar *lang;
		GType gdatype;

		gdatype = gda_holder_get_g_type (holder);
#ifdef HAVE_LC_MESSAGES
		lang = setlocale (LC_MESSAGES, NULL);
#else
		lang = setlocale (LC_CTYPE, NULL);
#endif
		while (vnode) {
			if (xmlNodeIsText (vnode)) {
				vnode = vnode->next;
				continue;
			}

			if (strcmp ((gchar*)vnode->name, "gda_value")) {
				vnode = vnode->next;
				continue;
			}

			/* don't care about entries for the wrong locale */
			this_lang = xmlGetProp(vnode, (xmlChar*)"lang");
			if (this_lang && strncmp ((gchar*)this_lang, lang, strlen ((gchar*)this_lang))) {
				g_free (this_lang);
				vnode = vnode->next;
				continue;
			}
			
			isnull = xmlGetProp(vnode, (xmlChar*)"isnull");
			if (isnull) {
				if ((*isnull == 'f') || (*isnull == 'F')) {
					xmlFree (isnull);
					isnull = NULL;
				}
			}
			
			if (!isnull) {
				GValue *value;
				gchar* nodeval = (gchar*)xmlNodeGetContent (vnode);

				value = g_new0 (GValue, 1);
				if (! gda_value_set_from_string (value, nodeval, gdatype)) {
					/* error */
					g_free (value);
				}
				else 
					gda_holder_take_value (holder, value);

 				xmlFree(nodeval);
			}
			else {
				gda_holder_set_value (holder, NULL);
				xmlFree (isnull);
			}
			
			vnode = vnode->next;
		}
	}
}


#define GDA_PARAM_ENCODE_TOKEN "__gda"
/**
 * gda_text_to_alphanum
 * @text:
 *
 * The "encoding" consists in replacing non
 * alphanumeric character with the string "__gdaXX" where XX is the hex. representation
 * of the non alphanumeric char.
 *
 * Returns: a new string
 */
gchar *
gda_text_to_alphanum (const gchar *text)
{
	GString *string;
	const gchar* ptr = text;
	gchar *ret;

	/*g_print ("%s (%s) ", __FUNCTION__, text);*/
	string = g_string_new ("");
	for (ptr = text; ptr && *ptr; ptr++) {
		if (! (((*ptr >= '0') && (*ptr <= '9')) ||
		       ((*ptr >= 'A') && (*ptr <= 'Z')) ||
		       ((*ptr >= 'a') && (*ptr <= 'z')))) {
			g_string_append (string, GDA_PARAM_ENCODE_TOKEN);
			g_string_append_printf (string, "%0x", *ptr);
		}
		else
			g_string_append_c (string, *ptr);
	}
	ret = string->str;
	g_string_free (string, FALSE);
	/*g_print ("=>#%s#\n", ret);*/
	return ret;
}

/**
 * gda_alphanum_to_text
 * @text:
 *
 * Does the opposite of gda_text_to_alphanum(), in the same string 
 *
 * Returns: @text if conversion succedded or %NULL if an error occurred
 */
gchar *
gda_alphanum_to_text (gchar *text)
{
	gchar* ptr = text;
	gint length = strlen (text);
	static gint toklength = 0;

	if (toklength == 0)
		toklength = strlen (GDA_PARAM_ENCODE_TOKEN);
	/*g_print ("%s (%s) ", __FUNCTION__, text);*/
	for (ptr = text; ptr && *ptr; ) {
		if ((length >= toklength + 2) && !strncmp (ptr, GDA_PARAM_ENCODE_TOKEN, toklength)) {
			gchar *ptr2 = ptr + toklength;
			char c = *ptr2;
			gchar val;
			if ((c >= 'a') && (c <= 'f'))
				val = (c - 'a' + 10) * 16;
			else if ((c >= '0') && (c <= '9'))
				val = (c - '0') * 16;
			else
				return NULL;
			c = *(ptr2+1);
			if ((c >= 'a') && (c <= 'f'))
				val += c - 'a' + 10;
			else if ((c >= '0') && (c <= '9'))
				val += c - '0';
			else
				return NULL;
			*ptr = val;
			ptr++;
			length -= toklength + 1;
			memmove (ptr, ptr + toklength + 1, length);
		}
		else {
			ptr ++;
			length --;
		}
	}
	/*g_print ("=>#%s#\n", text);*/
	return text;
}

/*
 * Checks related to the structure of the SELECT statement before computing the INSERT, UPDATE and DELETE
 * corresponding statements.
 */
static gboolean
dml_statements_check_select_structure (GdaConnection *cnc, GdaSqlStatement *sel_struct, GError **error)
{
	GdaSqlStatementSelect *stsel;
	stsel = (GdaSqlStatementSelect*) sel_struct->contents;
	if (!stsel->from) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     _("SELECT statement has no FROM part"));
		return FALSE;
	}
	if (stsel->from->targets && stsel->from->targets->next) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     _("SELECT statement involves more than one table or expression"));
		return FALSE;
	}
	GdaSqlSelectTarget *target;
	target = (GdaSqlSelectTarget*) stsel->from->targets->data;
	if (!target->table_name) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     _("SELECT statement involves more than one table or expression"));
		return FALSE;
	}
	if (!gda_sql_statement_check_validity (sel_struct, cnc, error)) 
		return FALSE;

	/* check that we want to modify a table */
	g_assert (target->validity_meta_object); /* because gda_sql_statement_check_validity() returned TRUE */
	if (target->validity_meta_object->obj_type != GDA_META_DB_TABLE) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     _("Can only build modification statement for tables"));
		return FALSE;
	}

	return TRUE;
}

/*
 * Builds the WHERE condition of the computed DML statement
 */
static GdaSqlExpr*
dml_statements_build_condition (GdaSqlStatementSelect *stsel, GdaMetaTable *mtable, gboolean require_pk, GError **error)
{
	gint i;
	GdaSqlExpr *expr;
	GdaSqlOperation *and_cond = NULL;

	if (mtable->pk_cols_nb == 0) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     _("Table does not have any primary key"));
		return NULL;
	}
	
	expr = gda_sql_expr_new (NULL); /* no parent */
	if (require_pk) {
		if (mtable->pk_cols_nb == 0) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					     _("Table does not have any primary key"));
			goto onerror;
		}
		else if (mtable->pk_cols_nb > 1) {
			and_cond = gda_sql_operation_new (GDA_SQL_ANY_PART (expr));
			and_cond->operator = GDA_SQL_OPERATOR_AND;
			expr->cond = and_cond;
		}
		for (i = 0; i < mtable->pk_cols_nb; i++) {
			GdaSqlSelectField *sfield = NULL;
			GdaMetaTableColumn *tcol;
			GSList *list;
			
			tcol = (GdaMetaTableColumn *) g_slist_nth_data (mtable->columns, mtable->pk_cols_array[i]);
			for (list = stsel->expr_list; list; list = list->next) {
				sfield = (GdaSqlSelectField *) list->data;
				if (sfield->validity_meta_table_column == tcol)
					break;
				else
					sfield = NULL;
			}
			if (!sfield) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					     _("Table's primary key is not selected"));
				goto onerror;
			}
			else {
				GdaSqlOperation *op;
				GdaSqlExpr *opexpr;
				GdaSqlParamSpec *pspec;

				/* equal condition */
				op = gda_sql_operation_new (GDA_SQL_ANY_PART (and_cond ? (gpointer)and_cond : (gpointer)expr));
				op->operator = GDA_SQL_OPERATOR_EQ;
				if (and_cond) 
					and_cond->operands = g_slist_append (and_cond->operands, op);
				else
					expr->cond = op;
				/* left operand */
				opexpr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
				g_value_set_string (opexpr->value = gda_value_new (G_TYPE_STRING), tcol->column_name);
				op->operands = g_slist_append (op->operands, opexpr);

				/* right operand */
				opexpr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
				pspec = g_new0 (GdaSqlParamSpec, 1);
				pspec->name = g_strdup_printf ("-%d", mtable->pk_cols_array[i]);
				pspec->g_type = tcol->gtype != G_TYPE_INVALID ? tcol->gtype: G_TYPE_STRING;
				pspec->type = g_strdup (gda_g_type_to_string (pspec->g_type));
				pspec->nullok = tcol->nullok;
				opexpr->param_spec = pspec;
				op->operands = g_slist_append (op->operands, opexpr);
			}
		}
	}
	return expr;
	
 onerror:
	gda_sql_expr_free (expr);
	return NULL;
}

/*
 * Statement computation from meta store 
 * @select_stmt: must be a SELECT statement (compound statements not handled)
 */
gboolean
gda_compute_dml_statements (GdaConnection *cnc, GdaStatement *select_stmt, gboolean require_pk, 
			    GdaStatement **insert, GdaStatement **update, GdaStatement **delete, GError **error)
{
	GdaSqlStatement *sel_struct;
	GdaSqlStatementSelect *stsel;
	GdaStatement *ret_insert = NULL;
	GdaStatement *ret_update = NULL;
	GdaStatement *ret_delete = NULL;
	GdaMetaStruct *mstruct;
	gboolean retval = TRUE;
	GdaSqlSelectTarget *target;

	GdaSqlStatementInsert *ist = NULL;
	GdaSqlStatementUpdate *ust = NULL;
	GdaSqlStatementDelete *dst = NULL;	

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (select_stmt), FALSE);
	g_return_val_if_fail (gda_statement_get_statement_type (select_stmt) == GDA_SQL_STATEMENT_SELECT, FALSE);

	/*g_print ("*** %s\n", gda_statement_serialize (select_stmt));*/

	g_object_get (G_OBJECT (select_stmt), "structure", &sel_struct, NULL);
	if (!dml_statements_check_select_structure (cnc, sel_struct, error)) {
		retval = FALSE;
		goto cleanup;
	}

	/* normalize the statement */
	if (!gda_sql_statement_normalize (sel_struct, cnc, error)) {
		retval = FALSE;
		goto cleanup;
	}

	mstruct = sel_struct->validity_meta_struct;
	g_assert (mstruct); /* because gda_sql_statement_check_validity() returned TRUE */

	/* check that the condition will be buildable */
	stsel = (GdaSqlStatementSelect*) sel_struct->contents;
	target = (GdaSqlSelectTarget*) stsel->from->targets->data;
	
	/* actual statement structure's computation */        
	if (insert) {
		ist = g_new0 (GdaSqlStatementInsert, 1);
		GDA_SQL_ANY_PART (ist)->type = GDA_SQL_ANY_STMT_INSERT;
		ist->table = gda_sql_table_new (GDA_SQL_ANY_PART (ist));
		ist->table->table_name = g_strdup ((gchar *) target->table_name);
	}
	
	if (update) {
		ust = g_new0 (GdaSqlStatementUpdate, 1);
		GDA_SQL_ANY_PART (ust)->type = GDA_SQL_ANY_STMT_UPDATE;
		ust->table = gda_sql_table_new (GDA_SQL_ANY_PART (ust));
		ust->table->table_name = g_strdup ((gchar *) target->table_name);
		ust->cond = dml_statements_build_condition (stsel, 
							    GDA_META_DB_OBJECT_GET_TABLE (target->validity_meta_object),
							    require_pk, error);
		if (!ust->cond) {
			retval = FALSE;
			goto cleanup;
		}
		GDA_SQL_ANY_PART (ust->cond)->parent = GDA_SQL_ANY_PART (ust);
	}
        
	if (delete) {
		dst = g_new0 (GdaSqlStatementDelete, 1);
		GDA_SQL_ANY_PART (dst)->type = GDA_SQL_ANY_STMT_DELETE;
		dst->table = gda_sql_table_new (GDA_SQL_ANY_PART (dst));
		dst->table->table_name = g_strdup ((gchar *) target->table_name);
		dst->cond = dml_statements_build_condition (stsel, 
							    GDA_META_DB_OBJECT_GET_TABLE (target->validity_meta_object),
							    require_pk, error);
		if (!dst->cond) {
			retval = FALSE;
			goto cleanup;
		}
		GDA_SQL_ANY_PART (dst->cond)->parent = GDA_SQL_ANY_PART (dst);
	}

	GSList *expr_list;
	gint colindex;
	GSList *insert_values_list = NULL;
	GHashTable *fields_hash; /* key = a table's field's name, value = we don't care */
	fields_hash = g_hash_table_new ((GHashFunc) gda_identifier_hash, (GEqualFunc) gda_identifier_equal);
	for (expr_list = stsel->expr_list, colindex = 0; 
	     expr_list;
	     expr_list = expr_list->next, colindex++) {
		GdaSqlSelectField *selfield = (GdaSqlSelectField *) expr_list->data;
		if ((selfield->validity_meta_object != target->validity_meta_object) ||
		    !selfield->validity_meta_table_column)
			continue;

		/* field to insert into */
		if (g_hash_table_lookup (fields_hash, selfield->field_name))
			continue;
		g_hash_table_insert (fields_hash, selfield->field_name, GINT_TO_POINTER (1));
		if (insert) {
			GdaSqlField *field;
			field = gda_sql_field_new (GDA_SQL_ANY_PART (ist));
			field->field_name = g_strdup (selfield->field_name);
			ist->fields_list = g_slist_append (ist->fields_list, field);
		}
		if (update) {
			GdaSqlField *field;
			field = gda_sql_field_new (GDA_SQL_ANY_PART (ust));
			field->field_name = g_strdup (selfield->field_name);
			ust->fields_list = g_slist_append (ust->fields_list, field);
		}

		/* parameter for the inserted value */
		GdaSqlParamSpec *pspec = g_new0 (GdaSqlParamSpec, 1);
		GdaSqlExpr *expr;
		GdaMetaTableColumn *tcol;

		tcol = selfield->validity_meta_table_column;
		if (insert) {
			pspec->name = g_strdup_printf ("+%d", colindex);
			pspec->g_type = tcol->gtype != G_TYPE_INVALID ? tcol->gtype: G_TYPE_STRING;
			pspec->type = g_strdup (gda_g_type_to_string (pspec->g_type));
			pspec->nullok = tcol->nullok;
			expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ist));
			expr->param_spec = pspec;
			insert_values_list = g_slist_append (insert_values_list, expr);
		}
		if (update) {
			pspec->name = g_strdup_printf ("+%d", colindex);
			pspec->g_type = tcol->gtype != G_TYPE_INVALID ? tcol->gtype: G_TYPE_STRING;
			pspec->type = g_strdup (gda_g_type_to_string (pspec->g_type));
			pspec->nullok = tcol->nullok;
			expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ust));
			expr->param_spec = pspec;
			ust->expr_list = g_slist_append (ust->expr_list, expr);
		}
	}
	g_hash_table_destroy (fields_hash);

	/* finish the statements */
	if (insert) {
		GdaSqlStatement *st;

		if (!ist->fields_list) {
			/* nothing to insert => don't create statement */
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Could not compute any field to insert into"));
			retval = FALSE;
			goto cleanup;
		}
		ist->values_list = g_slist_append (NULL, insert_values_list);
		st = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);
		st->contents = ist;
		ist = NULL;
		ret_insert = g_object_new (GDA_TYPE_STATEMENT, "structure", st, NULL);
		gda_sql_statement_free (st);
	}
	if (update) {
		GdaSqlStatement *st;
		st = gda_sql_statement_new (GDA_SQL_STATEMENT_UPDATE);
		st->contents = ust;
		ust = NULL;
		ret_update = g_object_new (GDA_TYPE_STATEMENT, "structure", st, NULL);
		gda_sql_statement_free (st);
	}
	if (delete) {
		GdaSqlStatement *st;
		st = gda_sql_statement_new (GDA_SQL_STATEMENT_DELETE);
		st->contents = dst;
		dst = NULL;
		ret_delete = g_object_new (GDA_TYPE_STATEMENT, "structure", st, NULL);
		gda_sql_statement_free (st);
	}
	
 cleanup:
	gda_sql_statement_free (sel_struct);
	if (ist) {
		GdaSqlStatementContentsInfo *cinfo;
		cinfo = gda_sql_statement_get_contents_infos (GDA_SQL_STATEMENT_INSERT);
		cinfo->free (ist);
	}
	if (ust) {
		GdaSqlStatementContentsInfo *cinfo;
		cinfo = gda_sql_statement_get_contents_infos (GDA_SQL_STATEMENT_UPDATE);
		cinfo->free (ust);
	}
	if (dst) {
		GdaSqlStatementContentsInfo *cinfo;
		cinfo = gda_sql_statement_get_contents_infos (GDA_SQL_STATEMENT_DELETE);
		cinfo->free (dst);
	}
		
	if (insert)
		*insert = ret_insert;
	if (update)
		*update = ret_update;
	if (delete)
		*delete = ret_delete;
	return retval;
}

/*
 * computes a hash string from @is, to be used in hash tables as a GHashFunc
 */
guint
gda_identifier_hash (const gchar *id)
{
	const signed char *p = (signed char *) id;
	guint32 h = 0;
	gboolean lower = FALSE;

	if (*p != '"') {
		lower = TRUE;
		h = g_ascii_tolower (*p);
	}
	
	for (p += 1; *p && *p != '"'; p++) {
		if (lower)
			h = (h << 5) - h + g_ascii_tolower (*p);
		else
			h = (h << 5) - h + *p;
	}
	if (*p == '"' && *(p+1)) 
		g_warning ("Argument passed to %s() is not an SQL identifier", __FUNCTION__);

	return h;
}

/*
 * Does the same as strcmp(id1, id2), but handles the case where id1 and/or id2 are enclosed in double quotes,
 * can also be used in hash tables as a GEqualFunc
 */
gboolean
gda_identifier_equal (const gchar *id1, const gchar *id2)
{
	const gchar *ptr1, *ptr2;
	gboolean dq1 = FALSE, dq2 = FALSE;

	ptr1 = id1;
	if (*ptr1 == '"') {
		ptr1++;
		dq1 = TRUE;
	}
	ptr2 = id2;
	if (*ptr2 == '"') {
		ptr2++;
		dq2 = TRUE;
	}
	for (; *ptr1 && *ptr2; ptr1++, ptr2++) {
		gchar c1, c2;
		c1 = *ptr1;
		c2 = *ptr2;
		if (!dq1)
			c1 = g_ascii_tolower (c1);
		if (!dq2)
			c2 = g_ascii_tolower (c2);
		if (c1 != c2)
			return FALSE;
	}
	if (*ptr1 || *ptr2) {
		if (*ptr1 && (*ptr1 == '"'))
			return TRUE;
		if (*ptr2 && (*ptr2 == '"'))
			return TRUE;
		return FALSE;
	}
	return TRUE;
}
