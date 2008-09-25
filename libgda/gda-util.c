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
#include <sql-parser/gda-statement-struct-util.h>

#include <libgda/binreloc/gda-binreloc.h>

extern gchar *gda_lang_locale;

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
		else if (!g_ascii_strcasecmp (str, "time"))
			type = GDA_TYPE_TIME;
		else if (!g_ascii_strcasecmp (str, "timestamp"))
			type = GDA_TYPE_TIMESTAMP;
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
 * @model: a #GdaDataModel
 * @parent: the parent XML node
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @rows: an array containing which rows of @model will be exported, or %NULL for all rows
 * @nb_rows: the number of rows in @rows
 * @use_col_ids:
 *
 * Dump the data in a #GdaDataModel into a xmlNodePtr (as used in libxml).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_utility_data_model_dump_data_to_xml (GdaDataModel *model, xmlNodePtr parent, 
					 const gint *cols, gint nb_cols, 
					 const gint *rows, gint nb_rows,
					 gboolean use_col_ids)
{
	gboolean retval = TRUE;
	gint nrows, i;
	gint *rcols, rnb_cols;
	gchar **col_ids = NULL;
	xmlNodePtr data = NULL;

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

			g_free (id);
		}
	}

	/* add the model data to the XML output */
	if (!rows)
		nrows = gda_data_model_get_n_rows (model);
	else
		nrows = nb_rows;
	if (nrows > 0) {
		xmlNodePtr row;
		gint r, c;

		data = xmlNewChild (parent, NULL, (xmlChar*)"gda_array_data", NULL);
		for (r = 0; (r < nrows) && retval; r++) {
			row = xmlNewChild (data, NULL,  (xmlChar*)"gda_array_row", NULL);
			for (c = 0; c < rnb_cols; c++) {
				GValue *value;
				gchar *str = NULL;
				xmlNodePtr field = NULL;

				value = (GValue *) gda_data_model_get_value_at (model, rcols [c], rows ? rows [r] : r, NULL);
				if (!value) {
					retval = FALSE;
					break;
				}
				if (value && !gda_value_is_null ((GValue *) value)) { 
					if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
						str = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
					else if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
						if (g_value_get_string (value))
							str = gda_value_stringify (value);	
					}
					else
						str = gda_value_stringify (value);
				}
				if (!use_col_ids) {
					if (str && *str) 
						field = xmlNewTextChild (row, NULL,  (xmlChar*)"gda_value", (xmlChar*)str);
					else
						field = xmlNewChild (row, NULL,  (xmlChar*)"gda_value", NULL);
				}
				else {
					field = xmlNewTextChild (row, NULL,  (xmlChar*)"gda_array_value", (xmlChar*)str);
					xmlSetProp(field, (xmlChar*)"colid",  (xmlChar*)col_ids [c]);
				}

				if (!str)
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

	if (!retval) {
		xmlUnlinkNode (data);
		xmlFreeNode (data);
	}

	return retval;
}

/**
 * gda_utility_holder_load_attributes
 * @holder:
 * @node: an xmlNodePtr with a &lt;parameter&gt; tag
 * @sources: a list of #GdaDataModel
 * @error: a place to store errors, or %NULL
 *
 * Note: this method may set the "source" custom string property
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_utility_holder_load_attributes (GdaHolder *holder, xmlNodePtr node, GSList *sources, GError **error)
{
	xmlChar *str;
	xmlNodePtr vnode;
	gboolean retval = TRUE;

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
		g_object_set_data_full (G_OBJECT (holder), "source", str, xmlFree);

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
					if (gda_holder_set_source_model (holder, model, fno, error)) {
						gchar *str;
						/* rename the wrapper with the current holder's name */
						g_object_get (G_OBJECT (holder), "name", &str, NULL);
						g_object_set_data_full (G_OBJECT (model), "newname", str, g_free);
						g_object_get (G_OBJECT (holder), "description", &str, NULL);
						g_object_set_data_full (G_OBJECT (model), "newdescr", str, g_free);
					}
					else
						retval = FALSE;
				}
			}
		}
	}

	/* specified value */
	vnode = node->children;
	if (vnode) {
		xmlChar *this_lang, *isnull;
		const gchar *lang = gda_lang_locale;
		GType gdatype;

		gdatype = gda_holder_get_g_type (holder);
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
			this_lang = xmlGetProp (vnode, (xmlChar*)"lang");
			if (this_lang && strncmp ((gchar*)this_lang, lang, strlen ((gchar*)this_lang))) {
				xmlFree (this_lang);
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
				gchar* nodeval = (gchar*)xmlNodeGetContent (vnode);

				if (! gda_holder_set_value_str (holder, NULL, nodeval, error))
					retval = FALSE;
 				xmlFree(nodeval);
			}
			else {
				xmlFree (isnull);
				if (! gda_holder_set_value (holder, NULL, error))
					retval = FALSE;
			}
			
			vnode = vnode->next;
		}
	}

	return retval;
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
GdaSqlExpr*
gda_compute_unique_table_row_condition (GdaSqlStatementSelect *stsel, GdaMetaTable *mtable, gboolean require_pk, GError **error)
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
			and_cond->operator_type = GDA_SQL_OPERATOR_TYPE_AND;
			expr->cond = and_cond;
		}
		for (i = 0; i < mtable->pk_cols_nb; i++) {
			GdaSqlSelectField *sfield = NULL;
			GdaMetaTableColumn *tcol;
			GSList *list;
			gint index;
			
			tcol = (GdaMetaTableColumn *) g_slist_nth_data (mtable->columns, mtable->pk_cols_array[i]);
			for (index = 0, list = stsel->expr_list; 
			     list; 
			     index++, list = list->next) {
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
				op->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;
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
				pspec->name = g_strdup_printf ("-%d", index);
				pspec->g_type = tcol->gtype != G_TYPE_INVALID ? tcol->gtype: G_TYPE_STRING;
				pspec->nullok = tcol->nullok;
				opexpr->param_spec = pspec;
				op->operands = g_slist_append (op->operands, opexpr);
			}
		}
	}
	else {
		TO_IMPLEMENT;
		gda_sql_expr_free (expr);
		expr = NULL;
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
                            GdaStatement **insert_stmt, GdaStatement **update_stmt, GdaStatement **delete_stmt, GError **error)
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
	if (insert_stmt) {
		ist = g_new0 (GdaSqlStatementInsert, 1);
		GDA_SQL_ANY_PART (ist)->type = GDA_SQL_ANY_STMT_INSERT;
		ist->table = gda_sql_table_new (GDA_SQL_ANY_PART (ist));
		ist->table->table_name = g_strdup ((gchar *) target->table_name);
	}
	
	if (update_stmt) {
		ust = g_new0 (GdaSqlStatementUpdate, 1);
		GDA_SQL_ANY_PART (ust)->type = GDA_SQL_ANY_STMT_UPDATE;
		ust->table = gda_sql_table_new (GDA_SQL_ANY_PART (ust));
		ust->table->table_name = g_strdup ((gchar *) target->table_name);
		ust->cond = gda_compute_unique_table_row_condition (stsel, 
								    GDA_META_TABLE (target->validity_meta_object),
								    require_pk, error);
		if (!ust->cond) {
			retval = FALSE;
			goto cleanup;
		}
		GDA_SQL_ANY_PART (ust->cond)->parent = GDA_SQL_ANY_PART (ust);
	}
        
	if (delete_stmt) {
		dst = g_new0 (GdaSqlStatementDelete, 1);
		GDA_SQL_ANY_PART (dst)->type = GDA_SQL_ANY_STMT_DELETE;
		dst->table = gda_sql_table_new (GDA_SQL_ANY_PART (dst));
		dst->table->table_name = g_strdup ((gchar *) target->table_name);
		dst->cond = gda_compute_unique_table_row_condition (stsel, 
								    GDA_META_TABLE (target->validity_meta_object),
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
		if (insert_stmt) {
			GdaSqlField *field;
			field = gda_sql_field_new (GDA_SQL_ANY_PART (ist));
			field->field_name = g_strdup (selfield->field_name);
			ist->fields_list = g_slist_append (ist->fields_list, field);
		}
		if (update_stmt) {
			GdaSqlField *field;
			field = gda_sql_field_new (GDA_SQL_ANY_PART (ust));
			field->field_name = g_strdup (selfield->field_name);
			ust->fields_list = g_slist_append (ust->fields_list, field);
		}

		/* parameter for the inserted value */
		GdaSqlExpr *expr;
		GdaMetaTableColumn *tcol;

		tcol = selfield->validity_meta_table_column;
		if (insert_stmt) {
			GdaSqlParamSpec *pspec = g_new0 (GdaSqlParamSpec, 1);
			pspec->name = g_strdup_printf ("+%d", colindex);
			pspec->g_type = tcol->gtype != G_TYPE_INVALID ? tcol->gtype: G_TYPE_STRING;
			pspec->nullok = tcol->nullok;
			expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ist));
			expr->param_spec = pspec;
			insert_values_list = g_slist_append (insert_values_list, expr);
		}
		if (update_stmt) {
			GdaSqlParamSpec *pspec = g_new0 (GdaSqlParamSpec, 1);
			pspec->name = g_strdup_printf ("+%d", colindex);
			pspec->g_type = tcol->gtype != G_TYPE_INVALID ? tcol->gtype: G_TYPE_STRING;
			pspec->nullok = tcol->nullok;
			expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ust));
			expr->param_spec = pspec;
			ust->expr_list = g_slist_append (ust->expr_list, expr);
		}
	}
	g_hash_table_destroy (fields_hash);

	/* finish the statements */
	if (insert_stmt) {
		GdaSqlStatement *st;

		if (!ist->fields_list) {
			/* nothing to insert => don't create statement */
			/* To translators: this error message occurs when no "INSERT INTO <table> (field1, ...)..." 
			 * SQL statement can be computed because no table field can be used */
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
	if (update_stmt) {
		GdaSqlStatement *st;
		st = gda_sql_statement_new (GDA_SQL_STATEMENT_UPDATE);
		st->contents = ust;
		ust = NULL;
		ret_update = g_object_new (GDA_TYPE_STATEMENT, "structure", st, NULL);
		gda_sql_statement_free (st);
	}
  	if (delete_stmt) {
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
		
	if (insert_stmt)
		*insert_stmt = ret_insert;
	if (update_stmt)
		*update_stmt = ret_update;
	if (delete_stmt)
		*delete_stmt = ret_delete;
	return retval;
}

/**
 * gda_identifier_hash
 * @id: an identifier string
 *
 * computes a hash string from @id, to be used in hash tables as a #GHashFunc
 *
 * Returns: a new hash
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

/**
 * gda_identifier_equal
 * @id1: an identifier string
 * @id2: an identifier string
 *
 * Does the same as strcmp(@id1, @id2), but handles the case where id1 and/or id2 are enclosed in double quotes.
 * can also be used in hash tables as a #GEqualFunc.
 *
 * Returns: TRUE if @id1 and @id2 are equal.
 */
gboolean
gda_identifier_equal (const gchar *id1, const gchar *id2)
{
	const gchar *ptr1, *ptr2;
	gboolean dq1 = FALSE, dq2 = FALSE;

	if ((!id1 && id2) || (id1 && !id2))
		return FALSE;
	if (!id1 && !id2)
		return TRUE;

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


static char *copy_ident (const gchar *ident);
static char *concat_ident (const gchar *prefix, const gchar *ident);

static gchar *sql_start_words[] = {
	"ALTER",
	"SELECT",
	"INSERT",
	"DELETE",
	"UPDATE",
	"CREATE",
	"DROP",
	"ALTER",
	"COMMENT",
	"BEGIN",
	"COMMIT",
	"ROLLBACK"
};

static gchar *sql_middle_words[] = {
	"FROM",
	"INNER",
	"JOIN",
	"LEFT",
	"OUTER",
	"RIGHT",
	"OUTER",
	"WHERE",
	"HAVING",
	"LIMIT",
	"AND",
	"OR",
	"NOT",
	"SET"
};

/**
 * gda_completion_list_get
 * @cnc: a #GdaConnection object
 * @sql: a partial SQL statement which is the context of the completion proposal
 * @start: starting position within @sql of the "token" to complete (starts at 0)
 * @end: ending position within @sql of the "token" to complete
 *
 * Creates an array of strings (terminated by a %NULL) corresponding to possible completions.
 * If no completion is available, then the returned array contains just one NULL entry, and
 * if it was not possible to try to compute a completions list, then %NULL is returned.
 *
 * Returns: a new array of strings, or %NULL (use g_strfreev() to free the returned array)
 */
gchar **
gda_completion_list_get (GdaConnection *cnc, const gchar *sql, gint start, gint end)
{
	GArray *compl = NULL;
	gchar *text;
	const GValue *cvalue;

	if (!cnc) 
		return NULL;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	if (end < start)
		return NULL;

	/* init */
	compl = g_array_new (TRUE, TRUE, sizeof (gchar *));
	text = g_new0 (gchar, end - start + 2);
	memcpy (text, sql + start, end - start + 1);
	text [end - start + 1] = 0;

	if (start == 0) {
		/* 
		 * start of a statement => complete with SQL start of statement words 
		 */
		gint len;
		gint i;
		len = strlen (text);
		for (i = 0; i < (sizeof (sql_start_words) / sizeof (gchar*)); i++) {
			gint clen = strlen (sql_start_words[i]);
			if (!g_ascii_strncasecmp (sql_start_words[i], text, MIN (clen, len))) {
				gchar *str;
				str = g_strdup (sql_start_words[i]);
				g_array_append_val (compl, str);
			}
		}
		goto compl_finished;
	}
	
	if (!*text)
		goto compl_finished;
	
	gchar *obj_schema, *obj_name;
	GValue *schema_value = NULL;
	
	if (!_split_identifier_string (g_strdup (text), &obj_schema, &obj_name) && 
	    !_split_identifier_string (g_strdup_printf ("%s\"", text), &obj_schema, &obj_name)) {
		if (text [strlen(text) - 1] == '.') {
			obj_schema = g_strdup (text);
			obj_schema [strlen(text) - 1] = 0;
			obj_name = g_strdup ("");
		}
		else
			goto compl_finished;
	}
	
	if (*obj_name == '"') 
		_remove_quotes (obj_name);
	
	if (obj_schema) {
		if (*obj_schema == '"') 
			_remove_quotes (obj_schema);
		g_value_take_string ((schema_value = gda_value_new (G_TYPE_STRING)), obj_schema);
	}
	
	/*
	 * complete with "table" or "schema.table"
	 */
	GdaDataModel *model;
	GdaMetaStore *store;
	store = gda_connection_get_meta_store (cnc);
	if (schema_value)
		model = gda_meta_store_extract (store, 
						"SELECT table_name FROM _tables WHERE table_schema = ##schema::string", 
						NULL, "schema", schema_value, NULL);
	else
		model = gda_meta_store_extract (store, 
						"SELECT table_name FROM _tables WHERE table_short_name != table_full_name",
						NULL);
	if (model) {
		gint i, nrows;
		gint len = strlen (obj_name);
		
		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; i < nrows; i++) {
			cvalue = gda_data_model_get_value_at (model, 0, i, NULL);
			if (cvalue) {
				const gchar *tname;
				tname = g_value_get_string (cvalue);
				if (!strncmp (tname, obj_name, len)) {
					gchar *str;
					if (schema_value) 
						str = concat_ident (obj_schema, tname);
					else
						str = copy_ident (tname);
					g_array_append_val (compl, str);
				}
			}
		}
		g_object_unref (model);
	}
	
	/*
	 * complete with "table.column"
	 */
	model = NULL;
	if (!schema_value)
		model = gda_meta_store_extract (store, 
						"SELECT column_name FROM _columns", NULL);
	if (model) {
		gint i, nrows;
		gint len = strlen (obj_name);
		
		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; i < nrows; i++) {
			cvalue = gda_data_model_get_value_at (model, 0, i, NULL);
			if (cvalue) {
				const gchar *cname;
				cname = g_value_get_string (cvalue);
				if (!strncmp (cname, obj_name, len)) {
					gchar *str;
					str = copy_ident (cname);
					g_array_append_val (compl, str);
				}
			}
		}
		g_object_unref (model);
	}
	
	/*
	 * complete with "schema.table"
	 */
	model = NULL;
	if (! schema_value)
		model = gda_meta_store_extract (store, "SELECT schema_name FROM _schemata", NULL);
	if (model) {
		gint i, nrows;
		gint len = strlen (obj_name);
		
		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; i < nrows; i++) {
			cvalue = gda_data_model_get_value_at (model, 0, i, NULL);
			if (cvalue) {
				const gchar *tname;
				tname = g_value_get_string (cvalue);
				if (!strncmp (tname, obj_name, len)) {
					char *str;
					GdaDataModel *m2;
					str = copy_ident (tname);
				
					m2 = gda_meta_store_extract (store, 
								     "SELECT table_name FROM _tables WHERE table_schema = ##schema::string", 
								     NULL, "schema", cvalue, NULL);
					if (m2) {
						gint i2, nrows2;
						nrows2 = gda_data_model_get_n_rows (m2);
						for (i2 = 0; i2 < nrows2; i2++) {
							cvalue = gda_data_model_get_value_at (m2, 0, i2, NULL);
							if (cvalue) {
								gchar *str2;
								tname = g_value_get_string (cvalue);
								str2 = concat_ident (str, tname);
								g_array_append_val (compl, str2);
							}
						}
						
						g_object_unref (m2);
					}
					free (str);
				}
			}
		}
		g_object_unref (model);
		if (compl->len > 0)
			goto compl_finished;
	}
	
	if (schema_value)
		gda_value_free (schema_value);
	g_free (obj_name);
	
	/* 
	 * middle of a statement and no completion yet => complete with SQL statement words 
	 */
	{
		gint len;
		gint i;
		len = strlen (text);
		for (i = 0; i < (sizeof (sql_middle_words) / sizeof (gchar*)); i++) {
			gint clen = strlen (sql_middle_words[i]);
			if (!g_ascii_strncasecmp (sql_middle_words[i], text, MIN (clen, len))) {
				gchar *str;
				str = g_strdup (sql_middle_words[i]);
				g_array_append_val (compl, str);
			}
		}
	}
	
 compl_finished:
	g_free (text);
	if (compl) {
		if (compl->len >= 1) {
			gchar **ptr;
			ptr = (gchar**) compl->data;
			g_array_free (compl, FALSE);
			return ptr;
		}
		else {
			g_array_free (compl, TRUE);
			return NULL;
		}
	}
	else
		return NULL;
}


static char *
copy_ident (const gchar *ident)
{
	char *str;
	gint tlen = strlen (ident);
	if (_identifier_needs_quotes (ident)) {
		str = malloc (sizeof (char) * (tlen + 3));
		*str = '"';
		strcpy (str+1, ident);
		str [tlen + 1] = '"';
		str [tlen + 2] = 0;
	}
	else {
		str = malloc (sizeof (char) * (tlen + 1));
		strcpy (str, ident);		
	}
	return str;
}

static char *
concat_ident (const char *prefix, const gchar *ident)
{
	char *str;
	gint tlen = strlen (ident);
	gint plen = 0;

	if (prefix)
		plen = strlen (prefix) + 1;

	if (_identifier_needs_quotes (ident)) {
		str = malloc (sizeof (char) * (plen + tlen + 3));
		if (prefix) {
			strcpy (str, prefix);
			str [plen - 1] = '.';
			str [plen] = '"';
			strcpy (str + plen + 1, ident);
		}
		else {
			*str = '"';
			strcpy (str+1, ident);
		}
		str [plen + tlen + 1] = '"';
		str [plen + tlen + 2] = 0;
	}
	else {
		str = malloc (sizeof (char) * (plen + tlen + 1));
		if (prefix) {
			strcpy (str, prefix);
			str [plen - 1] = '.';
			strcpy (str + plen, ident);
		}
		else
			strcpy (str, ident);
	}
	return str;
}

/*
 *  RFC 1738 defines that these characters should be escaped, as well
 *  any non-US-ASCII character or anything between 0x00 - 0x1F.
 */
static char rfc1738_unsafe_chars[] =
{
    (char) 0x3C,               /* < */
    (char) 0x3E,               /* > */
    (char) 0x22,               /* " */
    (char) 0x23,               /* # */
    (char) 0x25,               /* % */
    (char) 0x7B,               /* { */
    (char) 0x7D,               /* } */
    (char) 0x7C,               /* | */
    (char) 0x5C,               /* \ */
    (char) 0x5E,               /* ^ */
    (char) 0x7E,               /* ~ */
    (char) 0x5B,               /* [ */
    (char) 0x5D,               /* ] */
    (char) 0x60,               /* ` */
    (char) 0x27,               /* ' */
    (char) 0x20                /* space */
};

static char rfc1738_reserved_chars[] =
{
    (char) 0x3b,               /* ; */
    (char) 0x2f,               /* / */
    (char) 0x3f,               /* ? */
    (char) 0x3a,               /* : */
    (char) 0x40,               /* @ */
    (char) 0x3d,               /* = */
    (char) 0x26                /* & */
};

/**
 * gda_rfc1738_encode
 * @string: a string to encode 
 *
 * Encodes @string using the RFC 1738 recommendations: the
 * <constant>&lt;&gt;&quot;#%{}|\^~[]&apos;`;/?:@=&amp;</constant> and space characters are replaced by 
 * <constant>&quot;%%ab&quot;</constant> where
 * <constant>ab</constant> is the hexadecimal number corresponding to the character.
 *
 * Returns: a new string
 */
gchar *
gda_rfc1738_encode (const gchar *string)
{
	gchar *ret, *wptr;
	const gchar *rptr;
	gint i;

	if (!string)
		return NULL;
	if (!*string)
		return g_strdup ("");

	ret = g_new0 (gchar, (strlen (string) * 3) + 1);
	for (wptr = ret, rptr = string; *rptr; rptr++) {
		gboolean enc = FALSE;

		/* RFC 1738 defines these chars as unsafe */
		for (i = 0; i < sizeof (rfc1738_reserved_chars) / sizeof (char); i++) {
			if (*rptr == rfc1738_reserved_chars [i]) {
				enc = TRUE;
				break;
			}
		}
		if (!enc) {
			for (i = 0; i < sizeof (rfc1738_unsafe_chars) / sizeof (char); i++) {
				if (*rptr == rfc1738_unsafe_chars [i]) {
					enc = TRUE;
					break;
				}
			}
		}
		if (!enc) {
			/* RFC 1738 says any control chars (0x00-0x1F) are encoded */
			if ((unsigned char) *rptr <= (unsigned char) 0x1F) 
				enc = TRUE;
			/* RFC 1738 says 0x7f is encoded */
			else if (*rptr == (char) 0x7F)
				enc = TRUE;
			/* RFC 1738 says any non-US-ASCII are encoded */
			else if (((unsigned char) *rptr >= (unsigned char) 0x80))
				enc = TRUE;
		}
		if (!enc && (*rptr == '=')) {
			/* also encode the '=' */
			enc = TRUE;
		}

		if (enc) {
			sprintf (wptr, "%%%02x", (unsigned char) *rptr);
			wptr += 3;
		}
		else {
			*wptr = *rptr;
			wptr++;
		}
	}
	return ret;
}

/**
 * gda_rfc1738_decode
 * @string: a string to encode 
 *
 * Decodes @string using the RFC 1738 recommendations: the
 * <constant>&lt;&gt;&quot;#%{}|\^~[]&apos;`;/?:@=&amp;</constant> and space characters are replaced by 
 * <constant>&quot;%%ab&quot;</constant> where
 * <constant>ab</constant> is the hexadecimal number corresponding to the character.
 *
 * @string should respect the RFC 1738 encoding. If this is not the case (for example if there
 * is a "%2z" because 2z is not an hexadecimal value), then the part with the problem
 * is not decoded, and the function returns FALSE.
 *
 * @string is decoded in place, no new string gets created.
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_rfc1738_decode (gchar *string)
{
	gchar *wptr, *rptr;
	gboolean retval = TRUE;

	if (!string || !*string)
		return TRUE;

	for (wptr = rptr = string; *rptr; wptr++, rptr++) {
		*wptr = *rptr;
		if (*rptr == '%') {
			rptr++;
			if ((((*rptr >= 'A') && (*rptr <= 'F')) ||
			     ((*rptr >= 'a') && (*rptr <= 'f')) ||
			     ((*rptr >= '0') && (*rptr <= '9'))) &&
			    (((rptr[1] >= 'A') && (rptr[1] <= 'F')) ||
			     ((rptr[1] >= 'a') && (rptr[1] <= 'f')) ||
			     ((rptr[1] >= '0') && (rptr[1] <= '9')))) {
				*wptr = 0;
				if ((*rptr >= 'A') && (*rptr <= 'F'))
					*wptr = *rptr - 'A' + 10;
				else if ((*rptr >= 'a') && (*rptr <= 'f'))
					*wptr = *rptr - 'a' + 10;
				else
					*wptr = *rptr - '0';
				rptr++;
				*wptr = *wptr << 4; /* multiply by 16 */
				if (((*rptr >= 'A') && (*rptr <= 'F')) ||
				    ((*rptr >= 'a') && (*rptr <= 'f')) ||
				    ((*rptr >= '0') && (*rptr <= '9'))) {
					if ((*rptr >= 'A') && (*rptr <= 'F'))
						*wptr += *rptr - 'A' + 10;
					else if ((*rptr >= 'a') && (*rptr <= 'f'))
						*wptr += *rptr - 'a' + 10;
					else
						*wptr += *rptr - '0';
				}
			}
			else {
				/* error */
				retval = FALSE;
				rptr--;
			}
		}
	}
	*wptr = 0;
	return TRUE;
}


/**
 * gda_dsn_split
 * @string: a string in the "[&lt;username&gt;[:&lt;password&gt;]@]&lt;DSN&gt;" form
 * @out_dsn: a place to store the new string containing the &lt;DSN&gt; part
 * @out_username: a place to store the new string containing the &lt;username&gt; part
 * @out_password: a place to store the new string containing the &lt;password&gt; part
 * @error: a place to store errors, or %NULL
 *
 * Extract the DSN, username and password from @string. in @string, the various parts are strings
 * which are expected to be encoded using an RFC 1738 compliant encoding. If they are specified, 
 * the returned username and password strings are correclty decoded.
 *
 * @out_username and @out_password may be set to %NULL depending on @string's format.
 */
void
gda_dsn_split (const gchar *string, gchar **out_dsn, gchar **out_username, gchar **out_password)
{
	const gchar *ptr;
	g_return_if_fail (string);
	g_return_if_fail (out_dsn);
	g_return_if_fail (out_username);
	g_return_if_fail (out_password);

	*out_dsn = NULL;
	*out_username = NULL;
	*out_password = NULL;
	for (ptr = string; *ptr; ptr++) {
		if (*ptr == '@') {
			const gchar *tmp = ptr;
			*out_dsn = g_strdup (ptr+1);
			for (ptr = string; ptr < tmp; ptr++) {
				if (*ptr == ':') {
					*out_username = g_strndup (string, ptr - string);
					*out_password = g_strndup (ptr+1, tmp - ptr - 1);
				}
			}
			if (!*out_username) 
				*out_username = g_strndup (string, tmp - string);
			break;
		}
	}
	if (!*out_dsn)
		*out_dsn = g_strdup (string);

	/* RFC 1738 decode username and password strings */
	gda_rfc1738_decode (*out_username);
	gda_rfc1738_decode (*out_password);
}

/**
 * gda_connection_string_split
 * @string: a string in the "[&lt;provider&gt;://][&lt;username&gt;[:&lt;password&gt;]@]&lt;connection_params&gt;" form
 * @out_cnc_params: a place to store the new string containing the &lt;connection_params&gt; part
 * @out_provider: a place to store the new string containing the &lt;provider&gt; part
 * @out_username: a place to store the new string containing the &lt;username&gt; part
 * @out_password: a place to store the new string containing the &lt;password&gt; part
 * @error: a place to store errors, or %NULL
 *
 * Extract the provider, connection parameters, username and password from @string. 
 * in @string, the various parts are strings
 * which are expected to be encoded using an RFC 1738 compliant encoding. If they are specified, 
 * the returned provider, username and password strings are correclty decoded.
 */
void
gda_connection_string_split (const gchar *string, gchar **out_cnc_params, gchar **out_provider, 
			     gchar **out_username, gchar **out_password)
{
	const gchar *ptr;
	const gchar *ap;
	g_return_if_fail (string);
	g_return_if_fail (out_cnc_params);
	g_return_if_fail (out_provider);
	g_return_if_fail (out_username);
	g_return_if_fail (out_password);

	*out_cnc_params = NULL;
	*out_provider = NULL;
	*out_username = NULL;
	*out_password = NULL;
	for (ap = ptr = string; *ptr; ptr++) {
		if ((ap == string) && (*ptr == '/') && (ptr[1] == '/')) {
			if ((ptr == string) || (ptr[-1] != ':')) {
				g_free (*out_cnc_params); *out_cnc_params = NULL;
				g_free (*out_provider); *out_provider = NULL;
				g_free (*out_username); *out_username = NULL;
				g_free (*out_password); *out_password = NULL;
				return;
			}
			*out_provider = g_strndup (string, ptr - string - 1);
			ap = ptr+2;
			ptr++;
		}

		if (*ptr == '@') {
			const gchar *tmp = ptr;
			*out_cnc_params = g_strdup (ptr+1);
			for (ptr = ap; ptr < tmp; ptr++) {
				if (*ptr == ':') {
					*out_username = g_strndup (ap, ptr - ap);
					*out_password = g_strndup (ptr+1, tmp - ptr - 1);
				}
			}
			if (!*out_username) 
				*out_username = g_strndup (ap, tmp - ap);
			break;
		}
	}
	if (!*out_cnc_params)
		*out_cnc_params = g_strdup (ap);

	/* RFC 1738 decode provider, username and password strings */
	gda_rfc1738_decode (*out_provider);
	gda_rfc1738_decode (*out_username);
	gda_rfc1738_decode (*out_password);
}

/**
 * gda_parse_iso8601_date
 * @gdate: a pointer to a #GDate structure which will be filled
 * @value: a string
 *
 * Extracts date parts from @value, and sets @gdate's contents
 *
 * Accepted date format is "YYYY-MM-DD".
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_parse_iso8601_date (GDate *gdate, const gchar *value)
{
	GDateYear year;
	GDateMonth month;
	GDateDay day;

	year = atoi (value);
	value += 5;
	month = atoi (value);
	value += 3;
	day = atoi (value);
	
	g_date_clear (gdate, 1);
	if (g_date_valid_dmy (day, month, year)) {
		g_date_set_dmy (gdate, day, month, year);
		return TRUE;
	}
	else
		return FALSE;
}

/**
 * gda_parse_iso8601_time
 * @timegda: a pointer to a #GdaTime structure which will be filled
 * @value: a string
 *
 * Extracts time parts from @value, and sets @timegda's contents
 *
 * Accepted date format is "HH:MM:SS[.ms][TZ]" where TZ is +hour or -hour
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_parse_iso8601_time (GdaTime *timegda, const gchar *value)
{
	timegda->hour = atoi (value);
	value += 3;
	timegda->minute = atoi (value);
	value += 3;
	timegda->second = atoi (value);
	value += 2;
	if (*value != '.') {
		timegda->fraction = 0;
	} else {
		gint ndigits = 0;
		gint64 fraction;

		value++;
		fraction = atol (value);
		while (*value && *value != '+') {
			value++;
			ndigits++;
		}

		while (ndigits < 3) {
			fraction *= 10;
			ndigits++;
		}

		while (fraction > 0 && ndigits > 3) {
			fraction /= 10;
			ndigits--;
		}
		
		timegda->fraction = fraction;
	}

	if (*value)
		timegda->timezone = atol (value) * 60 * 60;
	else
		timegda->timezone = 0;

	return TRUE;
}

/**
 * gda_parse_iso8601_timestamp
 * @timestamp: a pointer to a #GdaTimeStamp structure which will be filled
 * @value: a string
 *
 * Extracts date and time parts from @value, and sets @timestamp's contents
 *
 * Accepted date format is "YYYY-MM-DD HH:MM:SS[.ms][TZ]" where TZ is +hour or -hour
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_parse_iso8601_timestamp (GdaTimestamp *timestamp, const gchar *value)
{
	timestamp->year = atoi (value);
	value += 5;
	timestamp->month = atoi (value);
	value += 3;
	timestamp->day = atoi (value);
	value += 3;
	timestamp->hour = atoi (value);
	value += 3;
	timestamp->minute = atoi (value);
	value += 3;
	timestamp->second = atoi (value);
	value += 2;
	if (*value != '.') {
		timestamp->fraction = 0;
	} else {
		gint ndigits = 0;
		gint64 fraction;

		value++;
		fraction = atol (value);
		while (*value && *value != '+') {
			value++;
			ndigits++;
		}

		while (ndigits < 3) {
			fraction *= 10;
			ndigits++;
		}

		while (fraction > 0 && ndigits > 3) {
			fraction /= 10;
			ndigits--;
		}
		
		timestamp->fraction = fraction;
	}

	if (*value)
		timestamp->timezone = atol (value) * 60 * 60;
	else
		timestamp->timezone = 0;

	return TRUE;
}
