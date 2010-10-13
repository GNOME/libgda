/* GDA common library
 * Copyright (C) 1998 - 2010 The GNOME Foundation.
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

#undef GDA_DISABLE_DEPRECATED
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-column.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <libgda/sql-parser/gda-sql-statement.h>
#include <sql-parser/gda-statement-struct-util.h>
#include <libgda/gda-set.h>
#include <libgda/gda-blob-op.h>

#include <libgda/binreloc/gda-binreloc.h>

#define KEYWORDS_HASH_NO_STATIC
#include <libgda/sqlite/keywords_hash.h>
#include "keywords_hash.c" /* this one is dynamically generated */

extern gchar *gda_lang_locale;
extern GdaAttributesManager *gda_holder_attributes_manager;

#define PROV_CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))

/**
 * gda_g_type_to_string
 * @type: Type to convert from.
 *
 * Converts a GType to its string representation (use gda_g_type_from_string() for the
 * operation in the other direction).
 *
 * This function wraps g_type_name() but for common types it provides an easier to
 * understand and remember name. For Example the G_TYPE_STRING is converted to "string"
 * whereas g_type_name() converts it to "gchararray".
 *
 * Returns: the GDA's string representing the given #GType or the name
 * returned by #g_type_name.
 */
const gchar *
gda_g_type_to_string (GType type)
{
	if (type == GDA_TYPE_NULL)
		return "null";
	else if (type == G_TYPE_INT)
		return "int";
	else if (type == G_TYPE_STRING)
		return "string";
	else if (type == G_TYPE_DATE)
		return "date";			
	else if (type == GDA_TYPE_TIME)
		return "time";
	else if (type == GDA_TYPE_TIMESTAMP)
		return "timestamp";
	else if (type == G_TYPE_BOOLEAN)
		return "boolean";
	else if (type == GDA_TYPE_BLOB)
		return "blob";
	else if (type == GDA_TYPE_BINARY)
		return "binary";
	else 
		return g_type_name (type);
}

/**
 * gda_g_type_from_string
 * @str: the name of a #GType, as returned by gda_g_type_to_string().
 *
 * Converts a named type to ts GType type (also see the gda_g_type_to_string() function).
 *
 * This function is a wrapper around the g_type_from_name() function, but also recognizes
 * some type synonyms such as:
 * <itemizedlist>
 *   <listitem><para>"int" for G_TYPE_INT</para></listitem>
 *   <listitem><para>"string" for G_TYPE_STRING</para></listitem>
 *   <listitem><para>"date" for G_TYPE_DATE</para></listitem>
 *   <listitem><para>"time" for GDA_TYPE_TIME</para></listitem>
 *   <listitem><para>"timestamp" for GDA_TYPE_TIMESTAMP</para></listitem>
 *   <listitem><para>"boolean" for G_TYPE_BOOLEAN</para></listitem>
 *   <listitem><para>"blob" for GDA_TYPE_BLOB</para></listitem>
 *   <listitem><para>"binary" for GDA_TYPE_BINARY</para></listitem>
 *   <listitem><para>"null" for GDA_TYPE_NULL</para></listitem>
 * </itemizedlist>
 *
 * Returns: the #GType represented by the given @str, or #G_TYPE_INVALID if not found
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
		else if (!strcmp (str, "blob"))
			type = GDA_TYPE_BLOB;
		else if (!strcmp (str, "binary"))
			type = GDA_TYPE_BINARY;
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
 * occurrence of "'" with "''" and "\" with "\\"
 *
 * Returns: a new string
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
 * Do the reverse of gda_default_escape_string(): transforms any "''" into "'", any
 * "\\" into "\" and any "\'" into "'". 
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
 * @use_col_ids: set to %TRUE to add column ID information
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
					else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
						/* force reading the whole blob */
						const GdaBlob *blob = gda_value_get_blob (value);
						if (blob) {
							const GdaBinary *bin = &(blob->data);
							if (blob->op && 
							    (bin->binary_length != gda_blob_op_get_length (blob->op)))
								gda_blob_op_read_all (blob->op, (GdaBlob*) blob);
						}
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

	if(!cols)
		g_free(rcols);

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
 * gda_utility_data_model_find_column_description
 * @model: a #GdaDataSelect data model
 * @field_name: field name
 *
 * Finds the description of a field into Metadata from a #GdaDataModel.
 *
 * Returns: The field's description, or NULL if description is not set
 */
const gchar *
gda_utility_data_model_find_column_description (GdaDataSelect *model, const gchar *field_name)
{
	g_return_val_if_fail (GDA_IS_DATA_SELECT (model), NULL);
	g_return_val_if_fail (field_name, NULL);

	GdaConnection *connection = gda_data_select_get_connection ((GdaDataSelect *) model);

	GdaStatement *statement;
	g_object_get (G_OBJECT (model), "select-stmt", &statement, NULL);
	if (!statement)
		return NULL;

	GdaSqlStatement *sql_statement;
	g_object_get (G_OBJECT (statement), "structure", &sql_statement, NULL);
	g_object_unref (statement);

	if (!gda_sql_statement_check_validity (sql_statement, connection, NULL)) {
		gda_sql_statement_free (sql_statement);
		return NULL;
	}

	GSList *fields;
	for (fields = ((GdaSqlStatementSelect *) sql_statement->contents)->expr_list;
	     fields;
	     fields = fields->next) {

		GdaSqlSelectField *select_field = fields->data;
		if (select_field->validity_meta_table_column) {
			GdaMetaTableColumn *meta_table_column = select_field->validity_meta_table_column;

			if (! strcmp (meta_table_column->column_name, field_name)) {
				const GValue *gvalue = gda_meta_table_column_get_attribute
					(meta_table_column, GDA_ATTRIBUTE_DESCRIPTION);

				gda_sql_statement_free (sql_statement);
				return gvalue ? g_value_get_string (gvalue) : NULL;
			}
		}
	}

	gda_sql_statement_free (sql_statement);
	return NULL;
}

/**
 * gda_utility_holder_load_attributes
 * @holder: a #GdaHolder
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
	if (str) {
		GValue *value;
#define GDAUI_ATTRIBUTE_PLUGIN "__gdaui_attr_plugin"
                value = gda_value_new_from_string ((gchar*) str, G_TYPE_STRING);
		gda_holder_set_attribute_static (holder, GDAUI_ATTRIBUTE_PLUGIN, value);
		gda_value_free (value);
		xmlFree (str);
	}

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
		g_free (source);
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

			if (!strcmp ((gchar*)vnode->name, "attribute")) {
				xmlChar *att_name;
				att_name = xmlGetProp (vnode, (xmlChar*) "name");
				if (att_name) {
					GValue *value;
					g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), 
							    (gchar*) xmlNodeGetContent (vnode));
					gda_attributes_manager_set_full (gda_holder_attributes_manager,
									 (gpointer) holder,
									 (gchar*) att_name, value,
									 (GDestroyNotify) xmlFree);
					gda_value_free (value);
				}
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
 * @text: the text to convert
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
 * @text: a string
 *
 * Does the opposite of gda_text_to_alphanum(), in the same string 
 *
 * Returns: @text if conversion succeeded or %NULL if an error occurred
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
			     "%s", _("SELECT statement has no FROM part"));
		return FALSE;
	}
	if (stsel->from->targets && stsel->from->targets->next) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("SELECT statement involves more than one table or expression"));
		return FALSE;
	}
	GdaSqlSelectTarget *target;
	target = (GdaSqlSelectTarget*) stsel->from->targets->data;
	if (!target->table_name) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("SELECT statement involves more than one table or expression"));
		return FALSE;
	}
	if (!gda_sql_statement_check_validity (sel_struct, cnc, error)) 
		return FALSE;

	/* check that we want to modify a table */
	g_assert (target->validity_meta_object); /* because gda_sql_statement_check_validity() returned TRUE */
	if (target->validity_meta_object->obj_type != GDA_META_DB_TABLE) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("Can only build modification statement for tables"));
		return FALSE;
	}

	return TRUE;
}

/**
 * gda_compute_unique_table_row_condition_with_cnc
 * @cnc: a #GdaConnection, or %NULL
 * @stsel: a #GdaSqlSelectStatement
 * @mtable: a #GdaMetaTable
 * @require_pk: set to TRUE if a primary key ir required
 * @error: a place to store errors, or %NULL
 * 
 * Computes a #GdaSqlExpr expression which can be used in the WHERE clause of an UPDATE
 * or DELETE statement when a row from the result of the @stsel statement has to be modified.
 *
 * Returns: a new #GdaSqlExpr, or %NULL if an error occurred.
 *
 * Since: 4.0.3
 */
GdaSqlExpr*
gda_compute_unique_table_row_condition_with_cnc (GdaConnection *cnc, GdaSqlStatementSelect *stsel,
						 GdaMetaTable *mtable, gboolean require_pk, GError **error)
{
	gint i;
	GdaSqlExpr *expr;
	GdaSqlOperation *and_cond = NULL;

	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	if (mtable->pk_cols_nb == 0) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("Table does not have any primary key"));
		return NULL;
	}
	
	expr = gda_sql_expr_new (NULL); /* no parent */
	if (require_pk) {
		if (mtable->pk_cols_nb == 0) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					     "%s", _("Table does not have any primary key"));
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
					     "%s", _("Table's primary key is not selected"));
				goto onerror;
			}
			else {
				GdaSqlOperation *op;
				GdaSqlExpr *opexpr;
				GdaSqlParamSpec *pspec;

				/* equal condition */
				if (and_cond) {
					opexpr = gda_sql_expr_new (GDA_SQL_ANY_PART (and_cond));
					op = gda_sql_operation_new (GDA_SQL_ANY_PART (opexpr));
					opexpr->cond = op;
					and_cond->operands = g_slist_append (and_cond->operands, opexpr);
				}
				else {
					op = gda_sql_operation_new (GDA_SQL_ANY_PART (expr));
					expr->cond = op;
				}
				op->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;
				/* left operand */
				gchar *str;
				opexpr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
				str = gda_sql_identifier_quote (tcol->column_name, cnc, NULL, FALSE, FALSE);
				g_value_take_string (opexpr->value = gda_value_new (G_TYPE_STRING), str);

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

/**
 * gda_compute_unique_table_row_condition
 * @stsel: a #GdaSqlSelectStatement
 * @mtable: a #GdaMetaTable
 * @require_pk: set to TRUE if a primary key ir required
 * @error: a place to store errors, or %NULL
 * 
 * Computes a #GdaSqlExpr expression which can be used in the WHERE clause of an UPDATE
 * or DELETE statement when a row from the result of the @stsel statement has to be modified.
 *
 * Returns: a new #GdaSqlExpr, or %NULL if an error occurred.
 */
GdaSqlExpr*
gda_compute_unique_table_row_condition (GdaSqlStatementSelect *stsel, GdaMetaTable *mtable, gboolean require_pk, GError **error)
{
	return gda_compute_unique_table_row_condition_with_cnc (NULL, stsel, mtable, require_pk, error);
}

/**
 * gda_compute_dml_statements
 * @cnc: a #GdaConnection
 * @select_stmt: a SELECT #GdaStatement (compound statements not handled)
 * @require_pk: TRUE if the created statement have to use a primary key
 * @insert_stmt: a place to store the created INSERT statement, or %NULL
 * @update_stmt: a place to store the created UPDATE statement, or %NULL
 * @delete_stmt: a place to store the created DELETE statement, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Creates an INSERT, an UPDATE and a DELETE statement from a SELECT statement
 * using the database metadata available in @cnc's meta store.
 * 
 * returns: TRUE if no error occurred
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

	GdaSqlStatement *sql_ist = NULL;
        GdaSqlStatementInsert *ist = NULL;
        GdaSqlStatement *sql_ust = NULL;
        GdaSqlStatementUpdate *ust = NULL;
        GdaSqlStatement *sql_dst = NULL;
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
	gchar *tmp;
	tmp = gda_sql_identifier_quote (target->table_name, cnc, NULL, FALSE, FALSE);
	if (insert_stmt) {
		sql_ist = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);
		ist = (GdaSqlStatementInsert*) sql_ist->contents;
		g_assert (GDA_SQL_ANY_PART (ist)->type == GDA_SQL_ANY_STMT_INSERT);

		ist->table = gda_sql_table_new (GDA_SQL_ANY_PART (ist));
		ist->table->table_name = g_strdup ((gchar *) target->table_name);
	}
	
	if (update_stmt) {
		sql_ust = gda_sql_statement_new (GDA_SQL_STATEMENT_UPDATE);
		ust = (GdaSqlStatementUpdate*) sql_ust->contents;
		g_assert (GDA_SQL_ANY_PART (ust)->type == GDA_SQL_ANY_STMT_UPDATE);

		ust->table = gda_sql_table_new (GDA_SQL_ANY_PART (ust));
		ust->table->table_name = g_strdup (tmp);
		ust->cond = gda_compute_unique_table_row_condition_with_cnc (cnc, stsel, 
									     GDA_META_TABLE (target->validity_meta_object),
									     require_pk, error);
		if (!ust->cond) {
			retval = FALSE;
			*update_stmt = NULL;
			update_stmt = NULL; /* don't try anymore to build UPDATE statement */
		}
		else
			GDA_SQL_ANY_PART (ust->cond)->parent = GDA_SQL_ANY_PART (ust);
	}
        
	if (delete_stmt) {
		sql_dst = gda_sql_statement_new (GDA_SQL_STATEMENT_DELETE);
		dst = (GdaSqlStatementDelete*) sql_dst->contents;
		g_assert (GDA_SQL_ANY_PART (dst)->type == GDA_SQL_ANY_STMT_DELETE);

		dst->table = gda_sql_table_new (GDA_SQL_ANY_PART (dst));
		dst->table->table_name = g_strdup (tmp);
		if (update_stmt && ust->cond)
			dst->cond = gda_sql_expr_copy (ust->cond);
		else
			dst->cond = gda_compute_unique_table_row_condition_with_cnc (cnc, stsel, 
									     GDA_META_TABLE (target->validity_meta_object),
									     require_pk, error);
		if (!dst->cond) {
			retval = FALSE;
			*delete_stmt = NULL;
			delete_stmt = NULL; /* don't try anymore to build DELETE statement */
		}
		else
			GDA_SQL_ANY_PART (dst->cond)->parent = GDA_SQL_ANY_PART (dst);
	}
	g_free (tmp);

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
		if (!ist->fields_list) {
			/* nothing to insert => don't create statement */
			/* To translators: this error message occurs when no "INSERT INTO <table> (field1, ...)..." 
			 * SQL statement can be computed because no table field can be used */
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     "%s", _("Could not compute any field to insert into"));
			retval = FALSE;
		}
		else {
			ist->values_list = g_slist_append (NULL, insert_values_list);
			ret_insert = g_object_new (GDA_TYPE_STATEMENT, "structure", sql_ist, NULL);
		}
	}
	if (update_stmt)
		ret_update = g_object_new (GDA_TYPE_STATEMENT, "structure", sql_ust, NULL);
  	if (delete_stmt)
		ret_delete = g_object_new (GDA_TYPE_STATEMENT, "structure", sql_dst, NULL);
	
 cleanup:
	gda_sql_statement_free (sel_struct);
	if (sql_ist)
		gda_sql_statement_free (sql_ist);
	if (sql_ust)
		gda_sql_statement_free (sql_ust);
	if (sql_dst)
		gda_sql_statement_free (sql_dst);
		
	if (insert_stmt)
		*insert_stmt = ret_insert;
	if (update_stmt)
		*update_stmt = ret_update;
	if (delete_stmt)
		*delete_stmt = ret_delete;
	return retval;
}

/**
 * gda_compute_select_statement_from_update
 * @update_stmt: an UPDATE statement
 * @error: a place to store errors, or %NULL
 *
 * Computes a SELECT statement which selects all the rows the @update_stmt would update. Beware
 * however that this GdaSqlStatement does not select anything (ie it would be rendered as "SELECT FROM ... WHERE ...")
 * and before being usable, one needs to add some fields to actually select.
 *
 * Returns: a new #GdaStatement if no error occurred, or %NULL otherwise
 */
GdaSqlStatement *
gda_compute_select_statement_from_update (GdaStatement *update_stmt, GError **error)
{
	GdaSqlStatement *upd_stmt;
	GdaSqlStatement *sel_stmt;
	GdaSqlStatementUpdate *ust;
	GdaSqlStatementSelect *sst;

	g_return_val_if_fail (update_stmt, NULL);
	g_object_get (G_OBJECT (update_stmt), "structure", &upd_stmt, NULL);
	g_return_val_if_fail (upd_stmt, NULL);
	g_return_val_if_fail (upd_stmt->stmt_type == GDA_SQL_STATEMENT_UPDATE, NULL);
	
	ust = (GdaSqlStatementUpdate*) upd_stmt->contents;

	sel_stmt = gda_sql_statement_new (GDA_SQL_STATEMENT_SELECT);
	sst = (GdaSqlStatementSelect*) sel_stmt->contents;
	g_assert (GDA_SQL_ANY_PART (sst)->type == GDA_SQL_ANY_STMT_SELECT);

	if (!ust->table || !ust->table->table_name) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("Missing table name in UPDATE statement"));
		return NULL;
	}

	/* FROM */
	GdaSqlSelectTarget *target;
	sst->from = gda_sql_select_from_new (GDA_SQL_ANY_PART (sst));
	target = gda_sql_select_target_new (GDA_SQL_ANY_PART (sst->from));
	sst->from->targets = g_slist_prepend (NULL, target);
	target->expr = gda_sql_expr_new (GDA_SQL_ANY_PART (target));
	g_value_set_string ((target->expr->value = gda_value_new (G_TYPE_STRING)), ust->table->table_name);

	/* WHERE */
	if (ust->cond) {
		sst->where_cond = gda_sql_expr_copy (ust->cond);
		GDA_SQL_ANY_PART (sst->where_cond)->parent = GDA_SQL_ANY_PART (sst);
	}

	gda_sql_statement_free (upd_stmt);

	return sel_stmt;
}

static gboolean stmt_rewrite_insert_remove (GdaSqlStatementInsert *ins, GdaSet *params, GError **error);
static gboolean stmt_rewrite_insert_default_keyword (GdaSqlStatementInsert *ins, GdaSet *params, GError **error);
static gboolean stmt_rewrite_update_default_keyword (GdaSqlStatementUpdate *upd, GdaSet *params, GError **error);


/**
 * gda_statement_rewrite_for_default_values
 * @stmt: a #GdaStatement object
 * @params: a #GdaSet containing the variable's values to be bound when executing @stmt
 * @remove: set to %TRUE if DEFAULT fields are removed, of %FALSE if the "DEFAULT" keyword is used
 * @error: a place to store errors, or %NULL
 *
 * Rewrites @stmt and creates a new #GdaSqlStatement where all the variables which are to a DEFAULT value
 * (as returned by gda_holder_value_is_default()) are either removed from the statement (if @remove
 * is %TRUE) or replaced by the "DEFAULT" keyword (if @remove is %FALSE).
 *
 * This function is only usefull for database providers' implementations which have to deal with default
 * values when executing statements, and is only relevant in the case of INSERT or UPDATE statements
 * (in the latter case an error is returned if @remove is %TRUE).
 *
 * For example the <programlisting><![CDATA[INSERT INTO mytable (id, name) VALUES (23, ##name::string)]]></programlisting>
 * is re-written into <programlisting><![CDATA[INSERT INTO mytable (id, name) VALUES (23, DEFAULT)]]></programlisting>
 * if @remove is %FALSE and into <programlisting><![CDATA[INSERT INTO mytable (id) VALUES (23)]]></programlisting>
 * if @remove is %TRUE.
 *
 * Returns: a new #GdaSqlStatement, or %NULL if an error occurred
 *
 * Since: 4.2
 */
GdaSqlStatement *
gda_statement_rewrite_for_default_values (GdaStatement *stmt, GdaSet *params, gboolean remove, GError **error)
{
	GdaSqlStatement *sqlst;
	GdaSqlStatementType type;
	gboolean ok = FALSE;
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (GDA_IS_SET (params), NULL);
	type = gda_statement_get_statement_type (stmt);

	g_object_get (stmt, "structure", &sqlst, NULL);
	if (! gda_sql_statement_check_structure (sqlst, error)) {
		gda_sql_statement_free (sqlst);
		return NULL;		
	}

	switch (type) {
	case GDA_SQL_STATEMENT_INSERT:
		if (remove)
			ok = stmt_rewrite_insert_remove ((GdaSqlStatementInsert*) sqlst->contents,
							 params, error);
		else
			ok = stmt_rewrite_insert_default_keyword ((GdaSqlStatementInsert*) sqlst->contents,
								  params, error);
		break;
	case GDA_SQL_STATEMENT_UPDATE:
		if (remove)
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_DEFAULT_VALUE_HANDLING_ERROR,
				     _("Can't rewrite UPDATE statement to handle default values"));
		else
			ok = stmt_rewrite_update_default_keyword ((GdaSqlStatementUpdate*) sqlst->contents,
								  params, error);
		break;
	default:
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_DEFAULT_VALUE_HANDLING_ERROR,
			     "Can't rewrite statement is not INSERT or UPDATE");
		break;
	}

	if (ok)
		return sqlst;
	else {
		gda_sql_statement_free (sqlst);
		return NULL;
	}
}

/*
 * Modifies @ins
 * Returns: TRUE if rewrite is Ok
 */
static gboolean
stmt_rewrite_insert_remove (GdaSqlStatementInsert *ins, GdaSet *params, GError **error)
{
	if (!ins->values_list)
		/* nothing to do */
		return TRUE;

	if (ins->values_list->next) {
		TO_IMPLEMENT;
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_DEFAULT_VALUE_HANDLING_ERROR,
			     "Not yet implemented");
		return FALSE;
	}

	GSList *fields, *values;
	for (fields = ins->fields_list, values = (GSList*) ins->values_list->data;
	     fields && values; ){
		GdaHolder *h;
		GdaSqlExpr *expr = (GdaSqlExpr*) values->data;
		if (! expr->param_spec || ! expr->param_spec->is_param) {
			fields = fields->next;
			values = values->next;
			continue;
		}
		h = gda_set_get_holder (params, expr->param_spec->name);
		if (!h) {
			gchar *str;
			str = g_strdup_printf (_("Missing parameter '%s' to execute query"),
					       expr->param_spec->name);
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
				     "%s", str);
			g_free (str);
			return FALSE;
		}

		if (gda_holder_value_is_default (h)) {
			GSList *tmp;
			
			gda_sql_field_free ((GdaSqlField*) fields->data);
			tmp = fields->next;
			ins->fields_list = g_slist_delete_link (ins->fields_list, fields);
			fields = tmp;
			
			gda_sql_expr_free (expr);
			tmp = values->next;
			ins->values_list->data = g_slist_delete_link ((GSList*) ins->values_list->data,
								      values);
			values = tmp;
		}
		else {
			fields = fields->next;
			values = values->next;
		}
	}

	if (! ins->values_list->data) {
		g_slist_free (ins->values_list);
		ins->values_list = NULL;
	}

	return TRUE;
}

/*
 * Modifies @ins
 * Returns: TRUE if rewrite is Ok
 */
static gboolean
stmt_rewrite_insert_default_keyword (GdaSqlStatementInsert *ins, GdaSet *params, GError **error)
{
	GSList *llist;
	for (llist = ins->values_list; llist; llist = llist->next) {
		GSList *values;
		for (values = (GSList*) llist->data;
		     values;
		     values = values->next){
			GdaHolder *h;
			GdaSqlExpr *expr = (GdaSqlExpr*) values->data;
			if (! expr->param_spec || ! expr->param_spec->is_param)
				continue;
			h = gda_set_get_holder (params, expr->param_spec->name);
			if (!h) {
				gchar *str;
				str = g_strdup_printf (_("Missing parameter '%s' to execute query"),
						       expr->param_spec->name);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
					     "%s", str);
				g_free (str);
				return FALSE;
			}
			
			if (gda_holder_value_is_default (h)) {
				GdaSqlExpr *nexpr;
				nexpr = gda_sql_expr_new (GDA_SQL_ANY_PART (ins));
				g_value_set_string ((nexpr->value = gda_value_new (G_TYPE_STRING)),
						    "DEFAULT");
				gda_sql_expr_free ((GdaSqlExpr*) values->data);
				values->data = nexpr;
			}
		}
	}

	if (! ins->values_list->data) {
		g_slist_free (ins->values_list);
		ins->values_list = NULL;
	}

	return TRUE;
}

static gboolean
stmt_rewrite_update_default_keyword (GdaSqlStatementUpdate *upd, GdaSet *params, GError **error)
{
	GSList *values;
	for (values = upd->expr_list; values; values = values->next) {
		GdaHolder *h;
		GdaSqlExpr *expr = (GdaSqlExpr*) values->data;
		if (! expr->param_spec || ! expr->param_spec->is_param)
			continue;
		h = gda_set_get_holder (params, expr->param_spec->name);
		if (!h) {
			gchar *str;
			str = g_strdup_printf (_("Missing parameter '%s' to execute query"),
					       expr->param_spec->name);
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
				     "%s", str);
			g_free (str);
			return FALSE;
		}
		
		if (gda_holder_value_is_default (h)) {
			GdaSqlExpr *nexpr;
			nexpr = gda_sql_expr_new (GDA_SQL_ANY_PART (upd));
			g_value_set_string ((nexpr->value = gda_value_new (G_TYPE_STRING)),
					    "DEFAULT");
			gda_sql_expr_free ((GdaSqlExpr*) values->data);
			values->data = nexpr;
		}
	}

	return TRUE;
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

static gchar *
prepare_sql_identifier_for_compare (gchar *str)
{
	if (!str || (*str == '"'))
		return str;
	else {
		gchar *ptr;
		for (ptr = str; *ptr; ptr++)
			*ptr = g_ascii_tolower (*ptr);
		return str;
	}
}

static gint
cmp_func (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (*((gchar**) a), *((gchar**) b));
}

/**
 * gda_completion_list_get:
 * @cnc: a #GdaConnection object
 * @sql: a partial SQL statement which is the context of the completion proposal
 * @start: starting position within @sql of the "token" to complete (starts at 0)
 * @end: ending position within @sql of the "token" to complete
 *
 * Creates an array of strings (terminated by a %NULL) corresponding to possible completions.
 * If no completion is available, then the returned array contains just one NULL entry, and
 * if it was not possible to try to compute a completions list, then %NULL is returned.
 *
 * Returns: (transfer full) (array zero-terminated=1) (allow-none): a new array of strings, or %NULL (use g_strfreev() to free the returned array)
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
	if (!sql || !(*sql))
		return NULL;
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
		gsize len;
		gsize i;
		len = strlen (text);
		for (i = 0; i < (sizeof (sql_start_words) / sizeof (gchar*)); i++) {
			gsize clen = strlen (sql_start_words[i]);
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
		
	prepare_sql_identifier_for_compare (obj_name);
	if (obj_schema)
		g_value_take_string ((schema_value = gda_value_new (G_TYPE_STRING)),
				     prepare_sql_identifier_for_compare (obj_schema));
	
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
						str = g_strdup (tname);
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
					str = g_strdup (cname);
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
					str = g_strdup (tname);
				
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
					g_free (str);
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
		gsize len;
		gsize i;
		len = strlen (text);
		for (i = 0; i < (sizeof (sql_middle_words) / sizeof (gchar*)); i++) {
			gsize clen = strlen (sql_middle_words[i]);
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
			/* sort */
			gsize i;
			g_array_sort (compl, cmp_func);

			/* remove duplicates if any */
			for (i = 1; i < compl->len; ) {
				gchar *current, *before;
				current = g_array_index (compl, gchar*, i);
				before = g_array_index (compl, gchar*, i - 1);
				if (!strcmp (current, before)) {
					g_free (current);
					g_array_remove_index (compl, i);
				}
				else
					i++;
			}

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
concat_ident (const char *prefix, const gchar *ident)
{
	char *str;
	gint tlen = strlen (ident);
	gint plen = 0;

	if (prefix)
		plen = strlen (prefix) + 1;

	str = malloc (sizeof (char) * (plen + tlen + 1));
	if (prefix) {
		strcpy (str, prefix);
		str [plen - 1] = '.';
		strcpy (str + plen, ident);
	}
	else
		strcpy (str, ident);
	return str;
}

/**
 * gda_sql_identifier_split:
 * @id: an SQL identifier
 * 
 * Splits @id into an array of it sub parts. @id's format has to be "&lt;part&gt;[.&lt;part&gt;[...]]" where
 * each part is either a text surrounded by double quotes which can contain upper and lower cases or
 * an SQL identifier in lower case.
 *
 * For example the <![CDATA["test.\"ATable\""]]> string will result in the array: <![CDATA[{"test", "\"ATable\"", NULL}]]>
 *
 * Returns: (transfer full) (array zero-terminated=1) (allow-none): a new %NULL-terminated array of strings, or NULL (use g_strfreev() to free the returned array)
 */
gchar **
gda_sql_identifier_split (const gchar *id)
{
	gchar *copy;
	gchar *remain, *last;
	GArray *array = NULL;

	g_return_val_if_fail (id && *id, NULL);

	for (copy = g_strdup (id); copy; copy = remain) {
		if (_split_identifier_string (copy, &remain, &last)) {
			if (!array)
				array = g_array_new (TRUE, TRUE, sizeof (gchar *));
			g_array_prepend_val (array, last);
		}
	}

	if (array)
		return (gchar **) g_array_free (array, FALSE);
	else
		return NULL;
}

static gboolean _sql_identifier_needs_quotes (const gchar *str);

/**
 * gda_sql_identifier_quote
 * @id: an SQL identifier
 * @cnc: a #GdaConnection object, or %NULL
 * @prov: a #GdaServerProvider object, or %NULL
 * @for_meta_store set to %TRUE if the returned string will be used in a #GdaMetaStore
 * @force_quotes: set to %TRUE to force the returned string to be quoted
 *
 * Use this function for any SQL identifier to make sure that:
 * <itemizedlist>
 *   <listitem>
 *     <para>it is correctly formatted
 *           to be used with @cnc (if @cnc is %NULL, then some default SQL quoting rules will be applied,
 *           similar to PostgreSQL's way) if @for_meta_store is %FALSE;
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>it is correctly formatted to be used with the #GdaMetaStore's object associated to @cnc
 *           is @for_meta_store is %TRUE.
 *     </para>
 *   </listitem>
 * </itemizedlist>
 *
 * The @force_quotes allow some control of how to interpret @id: if %FALSE, then @id will be left
 * unchanged most of the time (except for example if it's a reserved keyword), otherwise
 * if @force_quotes is %TRUE, then the returned string will most probably have quotes around it
 * to request that the database keep the case sensitiveness (but again, this may vary depending
 * on the database being accessed through @cnc).
 *
 * For example, the following table gives the result of this function depending on the arguments
 * when @cnc is %NULL (and @prov is also %NULL):
 * <table frame="all">
 *  <tgroup cols="6" colsep="1" rowsep="1" align="justify">
 *    <thead>
 *      <row>
 *        <entry>id</entry>
 *        <entry>for_meta_store=%FALSE, force_quotes=%FALSE</entry>
 *        <entry>for_meta_store=%TRUE, force_quotes=%FALSE</entry>
 *        <entry>for_meta_store=%FALSE, force_quotes=%TRUE</entry>
 *        <entry>for_meta_store=%TRUE, force_quotes=%TRUE</entry>
 *        <entry>remark</entry>
 *      </row>
 *    </thead>
 *    <tbody>
 *      <row>
 *        <entry>"double word"</entry>
 *        <entry>"double word"</entry>
 *        <entry>"double word"</entry>
 *        <entry>"double word"</entry>
 *        <entry>"double word"</entry>
 *        <entry>non allowed character in SQL identifier</entry>
 *      </row>
 *      <row>
 *        <entry>"CapitalTest"</entry>
 *        <entry>"CapitalTest"</entry>
 *        <entry>"CapitalTest"</entry>
 *        <entry>"CapitalTest"</entry>
 *        <entry>"CapitalTest"</entry>
 *        <entry>Mixed case SQL identifier, already quoted</entry>
 *      </row>
 *      <row>
 *        <entry>CapitalTest</entry>
 *        <entry>CapitalTest</entry>
 *        <entry>capitaltest</entry>
 *        <entry>"CapitalTest"</entry>
 *        <entry>"CapitalTest"</entry>
 *        <entry>Mixed case SQL identifier, non quoted</entry>
 *      </row>
 *      <row>
 *        <entry>"mytable"</entry>
 *        <entry>"mytable"</entry>
 *        <entry>mytable</entry>
 *        <entry>"mytable"</entry>
 *        <entry>mytable</entry>
 *        <entry>All lowser case, quoted</entry>
 *      </row>
 *      <row>
 *        <entry>mytable</entry>
 *        <entry>mytable</entry>
 *        <entry>mytable</entry>
 *        <entry>"mytable"</entry>
 *        <entry>mytable</entry>
 *        <entry>All lowser case</entry>
 *      </row>
 *      <row>
 *        <entry>MYTABLE</entry>
 *        <entry>MYTABLE</entry>
 *        <entry>mytable</entry>
 *        <entry>"MYTABLE"</entry>
 *        <entry>"MYTABLE"</entry>
 *        <entry>All upper case</entry>
 *      </row>
 *      <row>
 *        <entry>"MYTABLE"</entry>
 *        <entry>"MYTABLE"</entry>
 *        <entry>"MYTABLE"</entry>
 *        <entry>"MYTABLE"</entry>
 *        <entry>"MYTABLE"</entry>
 *        <entry>All upper case, quoted</entry>
 *      </row>
 *      <row>
 *        <entry>desc</entry>
 *        <entry>"desc"</entry>
 *        <entry>"desc"</entry>
 *        <entry>"desc"</entry>
 *        <entry>"desc"</entry>
 *        <entry>SQL reserved keyword</entry>
 *      </row>
 *      <row>
 *        <entry>5ive</entry>
 *        <entry>"5ive"</entry>
 *        <entry>"5ive"</entry>
 *        <entry>"5ive"</entry>
 *        <entry>"5ive"</entry>
 *        <entry>SQL identifier starting with a digit</entry>
 *      </row>
 *    </tbody>
 *  </tgroup>
 * </table>
 *
 * Here are a few examples of when and how to use this function:
 * <itemizedlist>
 *   <listitem>
 *     <para>
 *       When creating a table, the user has entered the table name, this function can be used to
 *       create a valid SQL identifier from the user provided table name:
 *       <programlisting>
 * gchar *user_sqlid=...
 * gchar *valid_sqlid = gda_sql_identifier_quote (user_sqlid, cnc, NULL, FALSE, FALSE);
 * gchar *sql = g_strdup_printf ("CREATE TABLE %s ...", valid_sqlid);
 * g_free (valid_sqlid);
 *       </programlisting>
 *       Note that this is an illustration and creating a table should be sone using a #GdaServerOperation
 *       object.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *      When updating the meta data associated to a table which has been created with the code
 *      above:
 *      <programlisting>
 * GValue table_name_value = { 0 };
 * gchar* column_names[] = { (gchar*)"table_name" };
 * GValue* column_values[] = { &table_name_value };
 * GdaMetaContext mcontext = { (gchar*)"_tables", 1, column_names, column_values };
 * g_value_init (&amp;table_name_value, G_TYPE_STRING);
 * g_value_take_string (&amp;table_name_value, gda_sql_identifier_quote (user_sqlid, cnc, NULL, TRUE, FALSE);
 * gda_connection_update_meta_store (cnc, &amp;mcontext, NULL);
 * g_value_reset (&amp;table_name_value);
 *       </programlisting>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *      When using a #GdaMetaStruct object to fetch information about a table (which has been created with
 *      the code above):
 *      <programlisting>
 * GValue table_name_value = { 0 };
 * g_value_init (&amp;table_name_value, G_TYPE_STRING);
 * g_value_take_string (&amp;table_name_value, gda_sql_identifier_quote (user_sqlid, cnc, NULL, TRUE, FALSE);
 * GdaMetaDbObject *dbo;
 * dbo = gda_meta_struct_complement (mstruct, GDA_META_DB_TABLE, NULL, NULL, &amp;table_name_value, NULL);
 * g_value_reset (&amp;table_name_value);
 *       </programlisting>
 *     </para>
 *   </listitem>
 * </itemizedlist>
 *
 *
 * Note that @id must not be a composed SQL identifier (such as "mytable.mycolumn" which should be
 * treated as the "mytable" and "mycolumn" SQL identifiers). If unsure, use gda_sql_identifier_split().
 *
 * Also note that if @cnc is %NULL, then it's possible to pass an non %NULL @prov to have a result specific
 * to @prov.
 *
 * For more information, see the <link linkend="gen:sql_identifiers">SQL identifiers and abstraction</link> and
 * <link linkend="information_schema:sql_identifiers">SQL identifiers in meta data</link> sections.
 *
 * Returns: the representation of @id ready to be used in SQL statement, as a new string,
 *          or %NULL if @id is in a wrong format
 *
 * Since: 4.0.3
 */
gchar *
gda_sql_identifier_quote (const gchar *id, GdaConnection *cnc, GdaServerProvider *prov,
			  gboolean for_meta_store, gboolean force_quotes)
{
	g_return_val_if_fail (id && *id, NULL);
	if (prov)
		g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (prov), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		if (prov)
			g_return_val_if_fail (gda_connection_get_provider (cnc) == prov, NULL);
		else
			prov = gda_connection_get_provider (cnc);
	}

	if ((*id == '*') && (! id [1]))
	    return g_strdup (id);

	if (prov && PROV_CLASS (prov)->identifier_quote)
		return PROV_CLASS (prov)->identifier_quote (prov, cnc, id,
							    for_meta_store, force_quotes);

	if (for_meta_store) {
		gchar *tmp, *ptr;
		tmp = _remove_quotes (g_strdup (id));
		if (is_keyword (tmp)) {
			ptr = gda_sql_identifier_add_quotes (tmp);
			g_free (tmp);
			return ptr;
		}
		else if (force_quotes) {
			/* quote if non LC characters or digits at the 1st char or non allowed characters */
			for (ptr = tmp; *ptr; ptr++) {
				if (((*ptr >= 'a') && (*ptr <= 'z')) ||
				    ((*ptr >= '0') && (*ptr <= '9') && (ptr != tmp)) ||
				    (*ptr == '_'))
					continue;
				else {
					ptr = gda_sql_identifier_add_quotes (tmp);
					g_free (tmp);
					return ptr;
				}
			}
			return tmp;
		}
		else {
			for (ptr = tmp; *ptr; ptr++) {
				if (*id == '"') {
					if (((*ptr >= 'a') && (*ptr <= 'z')) ||
					    ((*ptr >= '0') && (*ptr <= '9') && (ptr != tmp)) ||
					    (*ptr == '_'))
						continue;
					else {
						ptr = gda_sql_identifier_add_quotes (tmp);
						g_free (tmp);
						return ptr;
					}
				}
				else if ((*ptr >= 'A') && (*ptr <= 'Z'))
					*ptr += 'a' - 'A';
				else if ((*ptr >= '0') && (*ptr <= '9') && (ptr == tmp)) {
					ptr = gda_sql_identifier_add_quotes (tmp);
					g_free (tmp);
					return ptr;
				}
			}
			return tmp;
		}
	}
	else {
		/* default SQL standard */
		if (*id == '"') {
			/* there are already some quotes */
			return g_strdup (id);
		}
		if (is_keyword (id) || _sql_identifier_needs_quotes (id) || force_quotes)
			return gda_sql_identifier_add_quotes (id);
		
		/* nothing to do */
		return g_strdup (id);
	}
}

static gboolean
_sql_identifier_needs_quotes (const gchar *str)
{
	const gchar *ptr;

	g_return_val_if_fail (str, FALSE);
	for (ptr = str; *ptr; ptr++) {
		/* quote if 1st char is a number */
		if ((*ptr <= '9') && (*ptr >= '0')) {
			if (ptr == str)
				return TRUE;
			continue;
		}
		if (((*ptr >= 'A') && (*ptr <= 'Z')) ||
		    ((*ptr >= 'a') && (*ptr <= 'z')))
			continue;

		if ((*ptr != '$') && (*ptr != '_') && (*ptr != '#'))
			return TRUE;
	}
	return FALSE;
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
	gsize i;

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
 *
 * Extract the DSN, username and password from @string. in @string, the various parts are strings
 * which are expected to be encoded using an RFC 1738 compliant encoding. If they are specified, 
 * the returned username and password strings are correctly decoded.
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
 *
 * Extract the provider, connection parameters, username and password from @string. 
 * in @string, the various parts are strings
 * which are expected to be encoded using an RFC 1738 compliant encoding. If they are specified, 
 * the returned provider, username and password strings are correctly decoded.
 *
 * For example all the following connection strings:
 * <programlisting><![CDATA[
PostgreSQL://meme:pass@DB_NAME=mydb;HOST=server
PostgreSQL://meme@DB_NAME=mydb;HOST=server;PASSWORD=pass
PostgreSQL://meme@DB_NAME=mydb;PASSWORD=pass;HOST=server
PostgreSQL://meme@PASSWORD=pass;DB_NAME=mydb;HOST=server
PostgreSQL://DB_NAME=mydb;HOST=server;USERNAME=meme;PASSWORD=pass
PostgreSQL://DB_NAME=mydb;HOST=server;PASSWORD=pass;USERNAME=meme
PostgreSQL://DB_NAME=mydb;USERNAME=meme;PASSWORD=pass;HOST=server
PostgreSQL://PASSWORD=pass;USERNAME=meme;DB_NAME=mydb;HOST=server
PostgreSQL://:pass@USERNAME=meme;DB_NAME=mydb;HOST=server
PostgreSQL://:pass@DB_NAME=mydb;HOST=server;USERNAME=meme]]></programlisting>
 *
 * will return the following new strings (double quotes added here to delimit strings):
 * <programlisting><![CDATA[
out_cnc_params: "DB_NAME=mydb;HOST=server"
out_provider: "PostgreSQL"
out_username: "meme"
out_password: "pass"]]></programlisting>
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

	if (*out_cnc_params) {
		gchar *pos;

		pos = g_strrstr (*out_cnc_params, "USERNAME=");
		if (pos) {
			for (ptr = pos + 9; ptr && *ptr != '\0' && *ptr != ';'; ptr++);
			if (ptr != pos + 9)
				*out_username = g_strndup (pos + 9, ptr - (pos + 9));

			if (*ptr)
				g_memmove (pos, ptr + 1, strlen (ptr));
			else
				*pos = 0;
			gchar *tmp;
			gint len;
			tmp = *out_cnc_params;
			len = strlen (tmp) - 1;
			if (tmp [len] == ';')
				tmp [len] = 0;
		}

		pos = g_strrstr (*out_cnc_params, "PASSWORD=");
		if (pos) {
			for (ptr = pos + 9; ptr && *ptr != '\0' && *ptr != ';'; ptr++);
			if (ptr != pos + 9)
				*out_password = g_strndup (pos + 9, ptr - (pos + 9));

			if (*ptr)
				g_memmove (pos, ptr + 1, strlen (ptr));
			else
				*pos = 0;
			gchar *tmp;
			gint len;
			tmp = *out_cnc_params;
			len = strlen (tmp) - 1;
			if (tmp [len] == ';')
				tmp [len] = 0;
		}
	}

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
