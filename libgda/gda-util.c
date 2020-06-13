/*
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 - 2004 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2003 Paisa Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2004 Caolan McNamara <caolanm@redhat.com>
 * Copyright (C) 2004 Jürg Billeter <j@bitron.ch>
 * Copyright (C) 2004 - 2013 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2005 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 - 2009 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2008 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2008 Phil Longstaff <plongstaff@rogers.com>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011-2018 Daniel Espinosa <esodan@gmail.com>
 * Copyright (C) 2018 - 2019 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#undef GDA_DISABLE_DEPRECATED

#define G_LOG_DOMAIN "GDA-util"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-server-provider-private.h>
#include <libgda/gda-column.h>
#include <libgda/gda-data-model-iter.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <libgda/sql-parser/gda-sql-statement.h>
#include <sql-parser/gda-statement-struct-util.h>
#include <libgda/gda-set.h>
#include <libgda/gda-blob-op.h>
#include <libgda/gda-statement-priv.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/binreloc/gda-binreloc.h>

#define KEYWORDS_HASH_NO_STATIC
#include <libgda/sqlite/keywords_hash.h>
#include "keywords_hash.code" /* this one is dynamically generated */

/*
 * Win32 adaptations
 */
#ifdef G_OS_WIN32
#define strtok_r(s,d,p) strtok(s,d)
#endif


/**
 * gda_g_type_to_string:
 * @type: Type to convert from.
 *
 * Converts a GType to its string representation (use gda_g_type_from_string() for the
 * operation in the other direction).
 *
 * This function wraps g_type_name() but for common types it provides an easier to
 * understand and remember name. For Example the G_TYPE_STRING is converted to "string"
 * whereas g_type_name() converts it to "gchararray".
 *
 * Returns: (transfer none): the GDA's string representing the given #GType or the name
 * returned by #g_type_name.
 */
const gchar *
gda_g_type_to_string (GType type)
{
	if (type == G_TYPE_INVALID)
    return "null";
	else if (type == GDA_TYPE_NULL)
		return "null";
	else if (type == G_TYPE_INT)
		return "int";
	else if (type == G_TYPE_STRING)
		return "string";
	else if (type == G_TYPE_DATE)
		return "date";			
	else if (type == GDA_TYPE_TIME)
		return "time";
	else if (type == G_TYPE_DATE_TIME)
		return "timestamp";
	else if (type == G_TYPE_BOOLEAN)
		return "boolean";
	else if (type == GDA_TYPE_BLOB)
		return "blob";
	else if (type == GDA_TYPE_BINARY)
		return "binary";
	else if (type == GDA_TYPE_TEXT)
		return "text";
  else
		return g_type_name (type);
}

/**
 * gda_g_type_from_string:
 * @str: the name of a #GType, as returned by gda_g_type_to_string().
 *
 * Converts a named type to ts GType type (also see the gda_g_type_to_string() function).
 *
 * This function is a wrapper around the g_type_from_name() function, but also recognizes
 * some type synonyms such as:
 * <itemizedlist>
 *   <listitem><para>"int" for G_TYPE_INT</para></listitem>
 *   <listitem><para>"uint" for G_TYPE_UINT</para></listitem>
 *   <listitem><para>"int64" for G_TYPE_INT64</para></listitem>
 *   <listitem><para>"uint64" for G_TYPE_UINT64</para></listitem>
 *   <listitem><para>"char" for G_TYPE_CHAR</para></listitem>
 *   <listitem><para>"uchar" for G_TYPE_UCHAR</para></listitem>
 *   <listitem><para>"short" for GDA_TYPE_SHORT</para></listitem>
 *   <listitem><para>"ushort" for GDA_TYPE_USHORT</para></listitem>
 *   <listitem><para>"string" for G_TYPE_STRING</para></listitem>
 *   <listitem><para>"date" for G_TYPE_DATE</para></listitem>
 *   <listitem><para>"time" for GDA_TYPE_TIME</para></listitem>
 *   <listitem><para>"timestamp" for G_TYPE_DATE_TIME</para></listitem>
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
		else if (!g_ascii_strcasecmp (str, "uint"))
			type = G_TYPE_UINT;
		else if (!g_ascii_strcasecmp (str, "string"))
			type = G_TYPE_STRING;
		else if (!g_ascii_strcasecmp (str, "text"))
			type = GDA_TYPE_TEXT;
		else if (!g_ascii_strcasecmp (str, "date"))
			type = G_TYPE_DATE;
		else if (!g_ascii_strcasecmp (str, "time"))
			type = GDA_TYPE_TIME;
		else if (!g_ascii_strcasecmp (str, "timestamp"))
			type = G_TYPE_DATE_TIME;
		else if (!strcmp (str, "boolean"))
                        type = G_TYPE_BOOLEAN;
		else if (!strcmp (str, "blob"))
			type = GDA_TYPE_BLOB;
		else if (!strcmp (str, "binary"))
			type = GDA_TYPE_BINARY;
		else if (!strcmp (str, "null"))
			type = GDA_TYPE_NULL;
		else if (!strcmp (str, "short"))
			type = GDA_TYPE_SHORT;
		else if (!strcmp (str, "ushort"))
			type = GDA_TYPE_USHORT;
		else if (!g_ascii_strcasecmp (str, "int64"))
			type = G_TYPE_INT64;
		else if (!g_ascii_strcasecmp (str, "uint64"))
			type = G_TYPE_UINT64;
		else if (!g_ascii_strcasecmp (str, "char"))
			type = G_TYPE_CHAR;
		else if (!g_ascii_strcasecmp (str, "uchar"))
			type = G_TYPE_UCHAR;
		else if (!g_ascii_strcasecmp (str, "gshort"))
			type = GDA_TYPE_SHORT;
		else if (!g_ascii_strcasecmp (str, "gushort"))
			type = GDA_TYPE_USHORT;
		else
			/* could not find a valid GType for @str */
			type = G_TYPE_INVALID;
	}

  return type;
}

/**
 * gda_default_escape_string:
 * @string: string to escape
 *
 * Escapes @string to make it understandable by a DBMS. The escape method is very common and replaces any
 * occurrence of "'" with "''" and "\" with "\\"
 *
 * Returns: (transfer full) (nullable): a new string
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
 * gda_default_unescape_string:
 * @string: string to unescape
 *
 * Do the reverse of gda_default_escape_string(): transforms any "''" into "'", any
 * "\\" into "\" and any "\'" into "'". 
 *
 * Returns: (transfer full) (nullable): a new unescaped string, or %NULL in an error was found in @string
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
				memmove (ptr+1, ptr+2, total - offset);
				offset += 2;
			}
			else {
				g_free (retval);
				return NULL;
			}
		}
		if (*ptr == '\\') {
			if (*(ptr+1) == '\\') {
				memmove (ptr+1, ptr+2, total - offset);
				offset += 2;
			}
			else {
				if (*(ptr+1) == '\'') {
					*ptr = '\'';
					memmove (ptr+1, ptr+2, total - offset);
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
 * gda_utility_check_data_model_v: (rename-to gda_utility_check_data_model)
 * @model: a #GdaDataModel object
 * @nbcols: the minimum requested number of columns. Lenght of @types
 * @types: (array length=nbcols): array with @nbcols length of type GType or null (if any data type is accepted)
 *
 * Check the column types of a GdaDataModel.
 *
 * Returns: %TRUE if the data model's columns match the provided data types and number
 *
 * Since: 4.2.6
 */
gboolean
gda_utility_check_data_model_v (GdaDataModel *model, gint nbcols, GType* types)
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
		i = 0;
		while ((i<nbcols) && retval) {
			att = gda_data_model_describe_column (model, i);
			mtype = gda_column_get_g_type (att);

			rtype = types [i];
			if (mtype != rtype) {
				retval = FALSE;
#ifdef GDA_DEBUG_NO
				g_print ("Column %d: Expected %s, got %s\n",
					 i, gda_g_type_to_string (rtype), gda_g_type_to_string (mtype));
#endif
			}
			i++;
		}
	}

	return retval;
}


/**
 * gda_utility_check_data_model:
 * @model: a #GdaDataModel object
 * @nbcols: the minimum requested number of columns
 * @...: @nbcols arguments of type GType or -1 (if any data type is accepted)
 *
 * Check the column types of a GdaDataModel.
 *
 * Returns: %TRUE if the data model's columns match the provided data types and number
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
 * gda_utility_data_model_dump_data_to_xml:
 * @model: a #GdaDataModel
 * @parent: the parent XML node
 * @cols: (nullable) (array length=nb_cols): an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @rows: (nullable) (array length=nb_rows): an array containing which rows of @model will be exported, or %NULL for all rows
 * @nb_rows: the number of rows in @rows
 * @use_col_ids: set to %TRUE to add column ID information
 *
 * Dump the data in a #GdaDataModel into a xmlNodePtr (as used in libxml).
 *
 * Warning: this function uses a #GdaDataModelIter iterator, and if @model does not offer a random access
 * (check using gda_data_model_get_access_flags()), the iterator will be the same as normally used
 * to access data in @model previously to calling this method, and this iterator will be moved (point to
 * another row).
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_utility_data_model_dump_data_to_xml (GdaDataModel *model, xmlNodePtr parent, 
					 const gint *cols, gint nb_cols, 
					 const gint *rows, gint nb_rows,
					 gboolean use_col_ids)
{
	gboolean retval = TRUE;
	gint *rcols, rnb_cols;
	gchar **col_ids = NULL;
	xmlNodePtr data = NULL;
	GdaDataModelIter *iter;

	/* compute columns if not provided */
	if (!cols) {
		gint i;
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
	iter = gda_data_model_create_iter (model);
	if (iter && (gda_data_model_iter_get_row (iter) == -1) && ! gda_data_model_iter_move_next (iter)) {
		g_object_unref (iter);
		iter = NULL;
	}
	if (iter) {
		xmlNodePtr row;
		
		data = xmlNewChild (parent, NULL, (xmlChar*)"gda_array_data", NULL);
		for (; retval && gda_data_model_iter_is_valid (iter); gda_data_model_iter_move_next (iter)) {
			gint c;
			if (rows) {
				gint r;
				for (r = 0; r < nb_rows; r++) { 
					if (gda_data_model_iter_get_row (iter) == rows[r])
						break;
				}
				if (r == nb_rows)
					continue;
			}

			row = xmlNewChild (data, NULL,  (xmlChar*)"gda_array_row", NULL);
			for (c = 0; c < rnb_cols; c++) {
				GValue *value;
				gchar *str = NULL;
				xmlNodePtr field = NULL;

				value = (GValue*) gda_data_model_iter_get_value_at (iter, rcols[c]);
				if (value && !gda_value_is_null ((GValue *) value)) { 
					if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
						str = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
					else if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
						if (g_value_get_string (value))
							str = gda_value_stringify (value);	
					}
					else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
						/* force reading the whole blob */
						GdaBlob *blob = (GdaBlob*)gda_value_get_blob (value);
						if (blob) {
							GdaBinary *bin = gda_blob_get_binary (blob);
							if (gda_blob_get_op (blob) && 
							    (gda_binary_get_size (bin) != gda_blob_op_get_length (gda_blob_get_op (blob))))
								gda_blob_op_read_all (gda_blob_get_op (blob), (GdaBlob*) blob);
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
		g_object_unref (iter);
	}

	if (!cols)
		g_free (rcols);

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
 * gda_utility_data_model_find_column_description:
 * @model: a #GdaDataSelect data model
 * @field_name: field name
 *
 * Finds the description of a field into Metadata from a #GdaDataModel.
 *
 * Returns: (transfer none) (nullable): The field's description, or NULL if description is not set
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
				gda_sql_statement_free (sql_statement);
				return meta_table_column->desc;
			}
		}
	}

	gda_sql_statement_free (sql_statement);
	return NULL;
}

/**
 * gda_utility_holder_load_attributes:
 * @holder: a #GdaHolder
 * @node: an xmlNodePtr with a &lt;parameter&gt; tag
 * @sources: (element-type Gda.DataModel) (nullable): a list of #GdaDataModel
 * @error: a place to store errors, or %NULL
 *
 * Note: this method may set the "source" custom string property
 *
 * Returns: %TRUE if no error occurred
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
		gda_holder_set_not_null (holder, TRUE);
	
	str = xmlGetProp (node, BAD_CAST "plugin");
	if (str) {
		GValue *value;
#define GDAUI_ATTRIBUTE_PLUGIN "__gdaui_attr_plugin"
		value = gda_value_new_from_string ((gchar*) str, G_TYPE_STRING);
		g_object_set (holder, "plugin", value, NULL);
		gda_value_free (value);
		xmlFree (str);
	}

	str = xmlGetProp (node, BAD_CAST "source");
	if (str) 
		g_object_set_data_full (G_OBJECT (holder), "source", str, xmlFree);

	/* set restricting source if specified */
	if (str && sources) {
		gchar *ptr1, *ptr2 = NULL, *tok = NULL;
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
				
				fno = atoi (ptr2); /* Flawfinder: ignore */
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
		const gchar *lang = setlocale(LC_ALL, NULL);

		while (vnode) {
			if (xmlNodeIsText (vnode)) {
				vnode = vnode->next;
				continue;
			}

			if (!strcmp ((gchar*)vnode->name, "attribute")) {
				xmlChar *att_name;
				att_name = xmlGetProp (vnode, (xmlChar*) "name");
				if (att_name) {
					g_object_set_data_full ((GObject*) holder, (const gchar*) att_name, g_strdup ((gchar*) xmlNodeGetContent (vnode)), (GDestroyNotify) g_free);
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
 * gda_text_to_alphanum:
 * @text: the text to convert
 *
 * The "encoding" consists in replacing non
 * alphanumeric character with the string "__gdaXX" where XX is the hex. representation
 * of the non alphanumeric char.
 *
 * Returns: (transfer full): a new string
 */
gchar *
gda_text_to_alphanum (const gchar *text)
{
	GString *string;
	const gchar* ptr = text;

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

	return g_string_free (string, FALSE);
}

/**
 * gda_alphanum_to_text:
 * @text: a string
 *
 * Does the opposite of gda_text_to_alphanum(), in the same string 
 *
 * Returns: (transfer full) (nullable): @text if conversion succeeded or %NULL if an error occurred
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
 * gda_compute_unique_table_row_condition_with_cnc:
 * @cnc: (nullable): a #GdaConnection, or %NULL
 * @stsel: a #GdaSqlSelectStatement
 * @mtable: a #GdaMetaTable
 * @require_pk: set to %TRUE if a primary key is required
 * @error: a place to store errors, or %NULL
 * 
 * Computes a #GdaSqlExpr expression which can be used in the WHERE clause of an UPDATE
 * or DELETE statement when a row from the result of the @stsel statement has to be modified.
 *
 * If @require_pk is %TRUE then this function will return a non %NULL #GdaSqlExpr only if it can
 * use a primary key of @mtable. If @require_pk is %FALSE, then it will try to use a primary key of @mtable,
 * and if none is available, it will use all the columns of @mtable to compute a condition statement.
 *
 * Returns: (transfer full) (nullable): a new #GdaSqlExpr, or %NULL if an error occurred.
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
	
	expr = gda_sql_expr_new (NULL); /* no parent */

	if (mtable->pk_cols_nb == 0) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("Table does not have any primary key"));
		if (require_pk)
			goto onerror;
		else
			goto allcolums;
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
				     "%s", _("Table's primary key is not part of SELECT"));
			if (require_pk)
				goto onerror;
			else
				goto allcolums;
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
			pspec->g_type = (tcol->gtype != GDA_TYPE_NULL) ? tcol->gtype: G_TYPE_STRING;
			pspec->nullok = tcol->nullok;
			opexpr->param_spec = pspec;
			op->operands = g_slist_append (op->operands, opexpr);
		}
	}
	return expr;

 allcolums:

	gda_sql_expr_free (expr);
	expr = gda_sql_expr_new (NULL); /* no parent */

	GSList *columns;
	if (! mtable->columns) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     "%s", _("Table does not have any column"));
		goto onerror;
	}
	else if (mtable->columns->next) {
		and_cond = gda_sql_operation_new (GDA_SQL_ANY_PART (expr));
		and_cond->operator_type = GDA_SQL_OPERATOR_TYPE_AND;
		expr->cond = and_cond;
	}
	for (columns = mtable->columns; columns; columns = columns->next) {
		GdaSqlSelectField *sfield = NULL;
		GdaMetaTableColumn *tcol;
		GSList *list;
		gint index;
		
		tcol = (GdaMetaTableColumn *) columns->data;
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
				     _("Table's column '%s' is not part of SELECT"),
				     tcol->column_name);
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
			pspec->g_type = (tcol->gtype != GDA_TYPE_NULL) ? tcol->gtype: G_TYPE_STRING;
			pspec->nullok = tcol->nullok;
			opexpr->param_spec = pspec;
			op->operands = g_slist_append (op->operands, opexpr);
		}
	}
	return expr;
	
 onerror:
	gda_sql_expr_free (expr);
	return NULL;
}

/**
 * gda_compute_unique_table_row_condition:
 * @stsel: a #GdaSqlSelectStatement
 * @mtable: a #GdaMetaTable
 * @require_pk: set to TRUE if a primary key ir required
 * @error: a place to store errors, or %NULL
 * 
 * Computes a #GdaSqlExpr expression which can be used in the WHERE clause of an UPDATE
 * or DELETE statement when a row from the result of the @stsel statement has to be modified.
 *
 * Returns: (transfer full) (nullable): a new #GdaSqlExpr, or %NULL if an error occurred.
 */
GdaSqlExpr*
gda_compute_unique_table_row_condition (GdaSqlStatementSelect *stsel, GdaMetaTable *mtable, gboolean require_pk, GError **error)
{
	return gda_compute_unique_table_row_condition_with_cnc (NULL, stsel, mtable, require_pk, error);
}

/**
 * gda_compute_dml_statements:
 * @cnc: a #GdaConnection
 * @select_stmt: a SELECT #GdaStatement (compound statements not handled)
 * @require_pk: TRUE if the created statement have to use a primary key
 * @insert_stmt: (nullable) (out callee-allocates): a place to store the created INSERT statement, or %NULL
 * @update_stmt: (nullable) (out callee-allocates): a place to store the created UPDATE statement, or %NULL
 * @delete_stmt: (nullable) (out callee-allocates): a place to store the created DELETE statement, or %NULL
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Creates an INSERT, an UPDATE and a DELETE statement from a SELECT statement
 * using the database metadata available in @cnc's meta store. Each statements are computed only if
 * the corresponding place to store the created statement is not %NULL.
 * 
 * returns: %TRUE if no error occurred
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
        
	if (retval && delete_stmt) {
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
	if (!retval)
		goto cleanup;

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
			pspec->g_type = (tcol->gtype != GDA_TYPE_NULL) ? tcol->gtype: G_TYPE_STRING;
			pspec->nullok = tcol->nullok;
			expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ist));
			if (tcol->default_value)
				g_value_set_string ((expr->value = gda_value_new (G_TYPE_STRING)),
						    tcol->default_value);
			else if (tcol->auto_incement)
				expr->value = gda_value_new_default (GDA_EXTRA_AUTO_INCREMENT);

			expr->param_spec = pspec;
			insert_values_list = g_slist_append (insert_values_list, expr);
		}
		if (update_stmt) {
			GdaSqlParamSpec *pspec = g_new0 (GdaSqlParamSpec, 1);
			pspec->name = g_strdup_printf ("+%d", colindex);
			pspec->g_type = (tcol->gtype != GDA_TYPE_NULL) ? tcol->gtype: G_TYPE_STRING;
			pspec->nullok = tcol->nullok;
			expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ust));
			if (tcol->default_value)
				g_value_set_string ((expr->value = gda_value_new (G_TYPE_STRING)),
						    tcol->default_value);
			else if (tcol->auto_incement)
				expr->value = gda_value_new_default (GDA_EXTRA_AUTO_INCREMENT);
			expr->param_spec = pspec;
			ust->expr_list = g_slist_append (ust->expr_list, expr);
		}
	}
	g_hash_table_destroy (fields_hash);

	/* finish the statements */
	if (insert_stmt) {
		if (!ist->fields_list) {
			/* nothing to insert => don't create statement */
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     /* To translators: this error message occurs when no "INSERT INTO <table> (field1, ...)..." 
				      * SQL statement can be computed because no table field can be used */
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
 * gda_compute_select_statement_from_update:
 * @update_stmt: an UPDATE statement
 * @error: a place to store errors, or %NULL
 *
 * Computes a SELECT statement which selects all the rows the @update_stmt would update. Beware
 * however that this #GdaSqlStatement does not select anything (ie it would be rendered as "SELECT FROM ... WHERE ...")
 * and before being usable, one needs to add some fields to actually select.
 *
 * Returns: (transfer full) (nullable): a new #GdaStatement if no error occurred, or %NULL otherwise
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

typedef struct  {
	GdaSqlAnyPart *contents;
	GdaSet *params;
	GSList *expr_list; /* contains a list of #GdaSqlExpr after
			    * gda_sql_any_part_foreach has been called */
} NullData;

static gboolean
null_param_foreach_func (GdaSqlAnyPart *part, NullData *data , GError **error)
{
	if ((part->type != GDA_SQL_ANY_EXPR) || !((GdaSqlExpr*) part)->param_spec)
		return TRUE;

	if (!part->parent ||
	    (part->parent->type != GDA_SQL_ANY_SQL_OPERATION) ||
	    ((((GdaSqlOperation*) part->parent)->operator_type != GDA_SQL_OPERATOR_TYPE_EQ) &&
	     (((GdaSqlOperation*) part->parent)->operator_type != GDA_SQL_OPERATOR_TYPE_DIFF)))
		return TRUE;

	GdaHolder *holder;
	GdaSqlParamSpec *pspec = ((GdaSqlExpr*) part)->param_spec;
	holder = gda_set_get_holder (data->params, pspec->name);
	if (!holder)
		return TRUE;
	
	const GValue *cvalue;
	cvalue = gda_holder_get_value (holder);
	if (!cvalue || (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL))
		return TRUE;

	GdaSqlOperation *op;
	GdaSqlExpr *oexpr = NULL;
	op = (GdaSqlOperation*) part->parent;
	if (op->operands->data == part) {
		if (op->operands->next)
			oexpr = (GdaSqlExpr*) op->operands->next->data;
	}
	else
		oexpr = (GdaSqlExpr*) op->operands->data;
	if (oexpr && !g_slist_find (data->expr_list, oexpr)) /* handle situations where ##p1==##p2
							      * and both p1 and p2 are set to NULL */
		data->expr_list = g_slist_prepend (data->expr_list, part);
	return TRUE;
}

static GdaSqlExpr *
get_prev_expr (GSList *expr_list, GdaSqlAnyPart *expr)
{
	GSList *list;
	for (list = expr_list; list && list->next; list = list->next) {
		if ((GdaSqlAnyPart*) list->next->data == expr)
			return (GdaSqlExpr*) list->data;
	}
	return NULL;
}

static gboolean
null_param_unknown_foreach_func (GdaSqlAnyPart *part, NullData *data, GError **error)
{
	GdaSqlExpr *expr;
	if ((part->type != GDA_SQL_ANY_EXPR) || !((GdaSqlExpr*) part)->param_spec)
		return TRUE;

	if (!part->parent || part->parent != data->contents)
		return TRUE;

	GdaHolder *holder;
	GdaSqlParamSpec *pspec = ((GdaSqlExpr*) part)->param_spec;
	holder = gda_set_get_holder (data->params, pspec->name);
	if (!holder)
		return TRUE;
	
	const GValue *cvalue;
	cvalue = gda_holder_get_value (holder);
	if (!cvalue || (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL))
		return TRUE;

	GSList *tmplist = NULL;
	for (expr = get_prev_expr (((GdaSqlStatementUnknown*) data->contents)->expressions, part);
	     expr;
	     expr = get_prev_expr (((GdaSqlStatementUnknown*) data->contents)->expressions, (GdaSqlAnyPart*) expr)) {
		gchar *str, *tmp;
		if (!expr->value || (G_VALUE_TYPE (expr->value) != G_TYPE_STRING))
			goto out;

		str = (gchar*) g_value_get_string (expr->value);
		if (!str || !*str) {
			tmplist = g_slist_prepend (tmplist, expr);
			continue;
		}
		for (tmp = str + strlen (str) - 1; tmp >= str; tmp --) {
			if ((*tmp == ' ') || (*tmp == '\t') || (*tmp == '\n') || (*tmp == '\r'))
				continue;
			if (*tmp == '=') {
				gchar *dup;
				if ((tmp > str) && (*(tmp-1) == '!')) {
					*(tmp-1) = 0;
					dup = g_strdup_printf ("%s IS NOT NULL", str);
				}
				else {
					*tmp = 0;
					dup = g_strdup_printf ("%s IS NULL", str);
				}
				g_value_take_string (expr->value, dup);
				if (tmplist) {
					data->expr_list = g_slist_concat (tmplist, data->expr_list);
					tmplist = NULL;
				}
				data->expr_list = g_slist_prepend (data->expr_list, part);
				goto out;
			}
			else
				goto out;
		}
		tmplist = g_slist_prepend (tmplist, expr);
	}
 out:
	g_slist_free (tmplist);

	return TRUE;
}

/**
 * gda_rewrite_sql_statement_for_null_parameters:
 * @sqlst: (transfer full): a #GdaSqlStatement
 * @params: a #GdaSet to be used as parameters when executing @stmt
 * @out_modified: (nullable) (out): a place to store the boolean which tells if @stmt has been modified or not, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Modifies @sqlst to take into account any parameter which might be %NULL: if @sqlst contains the
 * equivalent of "xxx = &lt;parameter definition&gt;" and if that parameter is in @params and
 * its value is of type GDA_TYPE_NUL, then that part is replaced with "xxx IS NULL". It also
 * handles the "xxx IS NOT NULL" transformation.
 *
 * If @out_modified is not %NULL, then it will be set to %TRUE if @sqlst has been modified
 * by this function, and to %FALSE otherwise.
 *
 * This function is used by provider's implementations to make sure one can use parameters with
 * NULL values in statements without having to rewrite statements, as database usually don't
 * consider that "xxx = NULL" is the same as "xxx IS NULL" when using parameters.
 *
 * Returns: (transfer full) (nullable): the modified @sqlst statement, or %NULL if an error occurred
 *
 * Since: 4.2.9
 */
GdaSqlStatement *
gda_rewrite_sql_statement_for_null_parameters (GdaSqlStatement *sqlst, GdaSet *params,
					       gboolean *out_modified, GError **error)
{
	if (out_modified)
		*out_modified = FALSE;
	g_return_val_if_fail (sqlst, sqlst);

	if (!params)
		return sqlst;
	GSList *list;
	for (list = gda_set_get_holders (params); list; list = list->next) {
		const GValue *cvalue;
		cvalue = gda_holder_get_value ((GdaHolder*) list->data);
		if (cvalue && (G_VALUE_TYPE (cvalue) == GDA_TYPE_NULL))
			break;
	}
	if (!list || (sqlst->stmt_type == GDA_SQL_STATEMENT_NONE))
		return sqlst; /* no modifications necessary */

	NullData data;
	data.contents = GDA_SQL_ANY_PART (sqlst->contents);
	data.params = params;
	data.expr_list = NULL;

	if (sqlst->stmt_type == GDA_SQL_STATEMENT_UNKNOWN) {
		if (! gda_sql_any_part_foreach (GDA_SQL_ANY_PART (sqlst->contents),
						(GdaSqlForeachFunc) null_param_unknown_foreach_func,
						&data, error)) {
			gda_sql_statement_free (sqlst);
			return NULL;
		}
		if (out_modified)
			*out_modified = data.expr_list ? TRUE : FALSE;
		for (list = data.expr_list; list; list = list->next) {
			((GdaSqlStatementUnknown*) data.contents)->expressions = 
				g_slist_remove (((GdaSqlStatementUnknown*) data.contents)->expressions,
						list->data);
			gda_sql_expr_free ((GdaSqlExpr*) list->data);
		}
	}
	else {
		if (! gda_sql_any_part_foreach (GDA_SQL_ANY_PART (sqlst->contents),
						(GdaSqlForeachFunc) null_param_foreach_func,
						&data, error)) {
			gda_sql_statement_free (sqlst);
			return NULL;
		}
		if (out_modified)
			*out_modified = data.expr_list ? TRUE : FALSE;
		for (list = data.expr_list; list; list = list->next) {
			GdaSqlOperation *op;
			op = (GdaSqlOperation*) (((GdaSqlAnyPart*) list->data)->parent);
			op->operands = g_slist_remove (op->operands, list->data);
			if (op->operator_type == GDA_SQL_OPERATOR_TYPE_EQ)
				op->operator_type = GDA_SQL_OPERATOR_TYPE_ISNULL;
			else
				op->operator_type = GDA_SQL_OPERATOR_TYPE_ISNOTNULL;
			gda_sql_expr_free ((GdaSqlExpr*) list->data);
		}
	}
	g_slist_free (data.expr_list);
	return sqlst;
}

/**
 * gda_rewrite_statement_for_null_parameters:
 * @stmt: (transfer none): a #GdaStatement
 * @params: a #GdaSet to be used as parameters when executing @stmt
 * @out_stmt: (transfer full) (nullable) (out): a place to store the new #GdaStatement, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Modifies @stmt to take into account any parameter which might be %NULL: if @stmt contains the
 * equivalent of "xxx = &lt;parameter definition&gt;" and if that parameter is in @params and
 * its value is of type GDA_TYPE_NUL, then that part is replaced with "xxx IS NULL". It also
 * handles the "xxx IS NOT NULL" transformation.
 *
 * For example the following SELECT:
 * <programlisting>SELECT * FROM data WHERE id = ##id::int::null AND name = ##name::string</programlisting>
 * in case the "id" parameter is set to NULL, is converted to:
 * <programlisting>SELECT * FROM data WHERE id IS NULL AND name = ##name::string</programlisting>
 *
 * if @out_stmt is not %NULL, then it will contain:
 * <itemizedlist>
 *   <listitem><para>the modified statement if some modifications were required and no error occured (the function returns %TRUE)</para></listitem>
 *   <listitem><para>%NULL if no modification to @stmt were required and no erro occurred (the function returns %FALSE)</para></listitem>
 *   <listitem><para>%NULL if an error occured (the function returns %TRUE)</para></listitem>
 * </itemizedlist>
 *
 * This function is used by provider's implementations to make sure one can use parameters with
 * NULL values in statements without having to rewrite statements, as database usually don't
 * consider that "xxx = NULL" is the same as "xxx IS NULL" when using parameters.
 *
 * Returns: %TRUE if @stmt needs to be transformed to handle NULL parameters, and %FALSE otherwise
 *
 * Since: 4.2.9
 */
gboolean
gda_rewrite_statement_for_null_parameters (GdaStatement *stmt, GdaSet *params,
					   GdaStatement **out_stmt, GError **error)
{
	GdaSqlStatement *sqlst;
	gboolean mod = FALSE;
	gboolean ret = FALSE;
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);
	g_return_val_if_fail (!params || GDA_IS_SET (params), FALSE);

	if (out_stmt)
		*out_stmt = NULL;
	if (!params)
		return FALSE;

	g_object_get ((GObject*) stmt, "structure", &sqlst, NULL);
	if (gda_rewrite_sql_statement_for_null_parameters (sqlst, params, &mod, error)) {
		if (out_stmt) {
			if (mod) {
				*out_stmt = g_object_new (GDA_TYPE_STATEMENT, "structure", sqlst, NULL);
        ret = mod;
			}
		}
	}
	gda_sql_statement_free (sqlst);
	/* error => leave *out_stmt to %NULL */
	return ret;
}



static gboolean stmt_rewrite_insert_remove (GdaSqlStatementInsert *ins, GdaSet *params, GError **error);
static gboolean stmt_rewrite_insert_default_keyword (GdaSqlStatementInsert *ins, GdaSet *params, GError **error);
static gboolean stmt_rewrite_update_default_keyword (GdaSqlStatementUpdate *upd, GdaSet *params, GError **error);


/**
 * gda_statement_rewrite_for_default_values:
 * @stmt: a #GdaStatement object
 * @params: a #GdaSet containing the variable's values to be bound when executing @stmt
 * @remove: set to %TRUE if DEFAULT fields are removed, of %FALSE if the "DEFAULT" keyword is used
 * @error: a place to store errors, or %NULL
 *
 * Rewrites @stmt and creates a new #GdaSqlStatement where all the variables which are to a DEFAULT value
 * (as returned by gda_holder_value_is_default()) are either removed from the statement (if @remove
 * is %TRUE) or replaced by the "DEFAULT" keyword (if @remove is %FALSE).
 *
 * This function is only useful for database providers' implementations which have to deal with default
 * values when executing statements, and is only relevant in the case of INSERT or UPDATE statements
 * (in the latter case an error is returned if @remove is %TRUE).
 *
 * For example the <programlisting><![CDATA[INSERT INTO mytable (id, name) VALUES (23, ##name::string)]]></programlisting>
 * is re-written into <programlisting><![CDATA[INSERT INTO mytable (id, name) VALUES (23, DEFAULT)]]></programlisting>
 * if @remove is %FALSE and into <programlisting><![CDATA[INSERT INTO mytable (id) VALUES (23)]]></programlisting>
 * if @remove is %TRUE.
 *
 * Returns: (transfer full) (nullable): a new #GdaSqlStatement, or %NULL if an error occurred
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
				     "%s", _("Can't rewrite UPDATE statement to handle default values"));
		else
			ok = stmt_rewrite_update_default_keyword ((GdaSqlStatementUpdate*) sqlst->contents,
								  params, error);
		break;
	default:
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_DEFAULT_VALUE_HANDLING_ERROR,
			     "%s", _("Can't rewrite statement which is not INSERT or UPDATE"));
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
			     "%s", "Not yet implemented");
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


static gboolean
foreach_modify_param_type (GdaSqlAnyPart *part, GdaDataModel *model, G_GNUC_UNUSED GError **error)
{
	if (part->type != GDA_SQL_ANY_EXPR)
		return TRUE;

	GdaSqlParamSpec *pspec;
	pspec = ((GdaSqlExpr*) part)->param_spec;
	if (!pspec || !pspec->name)
		return TRUE;

	if ((pspec->name [0] == '+') || (pspec->name [0] == '-')) {
		long int li;
		char *end;
		li = strtol ((pspec->name) + 1, &end, 10);
		if ((! *end) && (li <= G_MAXINT) && (li >= G_MININT) && (li < gda_data_model_get_n_columns (model)))  {
			GdaColumn *col;
			col = gda_data_model_describe_column (model, (gint) li);
			if (col && (gda_column_get_g_type (col) != GDA_TYPE_NULL))
				pspec->g_type = gda_column_get_g_type (col);
		}
	}
	return TRUE;
}

/*
 * _gda_modify_statement_param_types:
 * @stmt: a #GdaStatement
 * @model: a #GdaDataModel
 *
 * Modifies the parameters in @stmt which will be mapped to columns in @model (using the +&lt;colindex&gt; or
 * -&lt;colindex&gt; syntax) to map the column types of @model.
 *
 * Since: 5.2
 */
void
_gda_modify_statement_param_types (GdaStatement *stmt, GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_STATEMENT (stmt));
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	GdaSqlStatement *sqlst;

	sqlst = _gda_statement_get_internal_struct (stmt);
	if (!sqlst || !sqlst->contents)
		return;

	if ((sqlst->stmt_type == GDA_SQL_STATEMENT_INSERT) ||
	    (sqlst->stmt_type == GDA_SQL_STATEMENT_UPDATE) ||
	    (sqlst->stmt_type == GDA_SQL_STATEMENT_DELETE)) {
		GdaSqlAnyPart *top;
		top = (GdaSqlAnyPart*) sqlst->contents;
		gda_sql_any_part_foreach (top, (GdaSqlForeachFunc) foreach_modify_param_type, model, NULL);
	}
}

/**
 * gda_identifier_hash:
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
 * gda_identifier_equal:
 * @id1: an identifier string
 * @id2: an identifier string
 *
 * Does the same as strcmp(@id1, @id2), but handles the case where id1 and/or id2 are enclosed in double quotes.
 * can also be used in hash tables as a #GEqualFunc.
 *
 * Returns: %TRUE if @id1 and @id2 are equal.
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

static const gchar *sql_start_words[] = {
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

static const gchar *sql_middle_words[] = {
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
 * @sql: a partial SQL statement which is the context of the completion proposal, may also start with a "." for
 *       Gda's tools which use internal commands
 * @start: starting position within @sql of the "token" to complete (starts at 0)
 * @end: ending position within @sql of the "token" to complete
 *
 * Creates an array of strings (terminated by a %NULL) corresponding to possible completions.
 * If no completion is available, then the returned array contains just one NULL entry, and
 * if it was not possible to try to compute a completions list, then %NULL is returned.
 *
 * Returns: (transfer full) (array zero-terminated=1) (nullable): a new array of strings, or %NULL (use g_strfreev() to free the returned array)
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
	text = g_new (gchar, end - start + 2);
	memcpy (text, sql + start, end - start + 1); /* Flawfinder: ignore */
	text [end - start + 1] = 0;

	if (start == 0) {
		/* 
		 * start of a statement => complete with SQL start of statement words 
		 */
		gsize len;
		gsize i;
		len = strlen (text);
		for (i = 0; i < G_N_ELEMENTS (sql_start_words); i++) {
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
		for (i = 0; i < G_N_ELEMENTS (sql_middle_words); i++) {
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
		strcpy (str, prefix); /* Flawfinder: ignore */
		str [plen - 1] = '.';
		strcpy (str + plen, ident); /* Flawfinder: ignore */
	}
	else
		strcpy (str, ident); /* Flawfinder: ignore */
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
 * Returns: (transfer full) (array zero-terminated=1) (nullable): a new %NULL-terminated array of strings, or NULL (use g_strfreev() to free the returned array)
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
 * gda_sql_identifier_quote:
 * @id: an SQL identifier
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @prov: (nullable): a #GdaServerProvider object, or %NULL
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
 * Returns: (transfer full) (nullable): the representation of @id ready to be used in SQL statement, as a new string,
 *          or %NULL if @id is in a wrong format
 *
 * Since: 4.0.3
 */
gchar *
gda_sql_identifier_quote (const gchar *id, GdaConnection *cnc, GdaServerProvider *prov,
			  gboolean for_meta_store, gboolean force_quotes)
{
#if GDA_DEBUG
	test_keywords ();
#endif
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

	if (prov) {
		gchar *quoted;
		quoted = _gda_server_provider_identifier_quote (prov, cnc, id, for_meta_store, force_quotes);
		if (quoted)
			return quoted;
	}

	if (for_meta_store) {
		gchar *tmp, *ptr;
		tmp = _remove_quotes (g_strdup (id));
		if (is_keyword (tmp)) {
			ptr = gda_sql_identifier_force_quotes (tmp);
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
					ptr = gda_sql_identifier_force_quotes (tmp);
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
						ptr = gda_sql_identifier_force_quotes (tmp);
						g_free (tmp);
						return ptr;
					}
				}
				else if ((*ptr >= 'A') && (*ptr <= 'Z'))
					*ptr += 'a' - 'A';
				else if ((*ptr >= '0') && (*ptr <= '9') && (ptr == tmp)) {
					ptr = gda_sql_identifier_force_quotes (tmp);
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
			return gda_sql_identifier_force_quotes (id);
		
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
static const char rfc1738_unsafe_chars[] =
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

static const char rfc1738_reserved_chars[] =
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
 * gda_rfc1738_encode:
 * @string: a string to encode 
 *
 * Encodes @string using the RFC 1738 recommendations: the
 * <constant>&lt;&gt;&quot;#%{}|\^~[]&apos;`;/?:@=&amp;</constant> and space characters are replaced by 
 * <constant>&quot;%%ab&quot;</constant> where
 * <constant>ab</constant> is the hexadecimal number corresponding to the character.
 *
 * Returns: (transfer full) (nullable): a new string or %NULL
 */
gchar *
gda_rfc1738_encode (const gchar *string)
{
	if (!string)
		return NULL;

	if (!*string)
		return g_strdup ("");

	gchar *ret, *wptr;
	const gchar *rptr;
	gsize i;

	ret = g_new0 (gchar, (strlen (string) * 3) + 1);
	for (wptr = ret, rptr = string; *rptr; rptr++) {
		gboolean enc = FALSE;

		/* RFC 1738 defines these chars as unsafe */
		for (i = 0; i < G_N_ELEMENTS (rfc1738_reserved_chars); i++) {
			if (*rptr == rfc1738_reserved_chars [i]) {
				enc = TRUE;
				break;
			}
		}
		if (!enc) {
			for (i = 0; i < G_N_ELEMENTS (rfc1738_unsafe_chars); i++) {
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
			sprintf (wptr, "%%%02x", (unsigned char) *rptr); /* Flawfinder: ignore */
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
 * gda_rfc1738_decode:
 * @string: a string to decode
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
 * Returns: %TRUE if no error occurred.
 */
gboolean
gda_rfc1738_decode (gchar *string)
{
	gchar *wptr, *rptr;

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
				/* TODO: Actually return this? retval = FALSE; murrayc */
				rptr--;
			}
		}
	}
	*wptr = 0;
	return TRUE;
}


/**
 * gda_dsn_split:
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
 * gda_connection_string_split:
 * @string: a string in the "[&lt;provider&gt;://][&lt;username&gt;[:&lt;password&gt;]@]&lt;connection_params&gt;" form
 * @out_cnc_params: (out): a place to store the new string containing the &lt;connection_params&gt; part
 * @out_provider: (out): a place to store the new string containing the &lt;provider&gt; part
 * @out_username: (out): a place to store the new string containing the &lt;username&gt; part
 * @out_password: (out): (nullable): a place to store the new string containing the &lt;password&gt; part, or %NULL
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

	*out_cnc_params = NULL;
	*out_provider = NULL;
	*out_username = NULL;
	if (out_password)
		*out_password = NULL;
	for (ap = ptr = string; *ptr; ptr++) {
		if ((ap == string) && (*ptr == '/') && (ptr[1] == '/')) {
			if ((ptr == string) || (ptr[-1] != ':')) {
				g_free (*out_cnc_params); *out_cnc_params = NULL;
				g_free (*out_provider); *out_provider = NULL;
				g_free (*out_username); *out_username = NULL;
				if (out_password) {
					g_free (*out_password);
					*out_password = NULL;
				}
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
					if (out_password)
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

		pos = strstr (*out_cnc_params, "USERNAME=");
		while (pos) {
			if (((pos > *out_cnc_params) && (*(pos-1) == ';')) ||
			    (pos  == *out_cnc_params)) {
				for (ptr = pos + 9; ptr && *ptr != '\0' && *ptr != ';'; ptr++);
				if (ptr != pos + 9)
					*out_username = g_strndup (pos + 9, ptr - (pos + 9));
				
				if (*ptr)
					memmove (pos, ptr + 1, strlen (ptr));
				else
					*pos = 0;
				gchar *tmp;
				gint len;
				tmp = *out_cnc_params;
				len = strlen (tmp) - 1;
				if (tmp [len] == ';')
					tmp [len] = 0;
				break;
			}
			pos = strstr (pos + 9, "USERNAME=");
		}

		pos = strstr (*out_cnc_params, "PASSWORD=");
		while (pos) {
			if (((pos > *out_cnc_params) && (*(pos-1) == ';')) ||
			    (pos  == *out_cnc_params)) {
				for (ptr = pos + 9; ptr && *ptr != '\0' && *ptr != ';'; ptr++);
				if (ptr != pos + 9) {
					if (out_password)
						*out_password = g_strndup (pos + 9, ptr - (pos + 9));
				}
				
				if (*ptr)
					memmove (pos, ptr + 1, strlen (ptr));
				else
					*pos = 0;
				gchar *tmp;
				gint len;
				tmp = *out_cnc_params;
				len = strlen (tmp) - 1;
				if (tmp [len] == ';')
					tmp [len] = 0;
				break;
			}
			pos = strstr (pos +  9, "PASSWORD=");
		}
	}

	/* RFC 1738 decode provider, username and password strings */
	gda_rfc1738_decode (*out_provider);
	gda_rfc1738_decode (*out_username);
	if (out_password)
		gda_rfc1738_decode (*out_password);
}

/*
 * NB: @sep must not be zero
 */
static gboolean
_parse_formatted_date (GDate *gdate, const gchar *value, GDateDMY first, GDateDMY second, GDateDMY third, gchar sep,
		       const char **out_endptr)
{
	GDateYear year = G_DATE_BAD_YEAR;
	GDateMonth month = G_DATE_BAD_MONTH;
	GDateDay day = G_DATE_BAD_DAY;
	unsigned long int tmp;
	const char *endptr;
	guint8 iter;
	guint16 parsed_values [3] = {0, 0, 0};

	g_date_clear (gdate, 1);

	/* checks */
	if ((first == second) || (first == third) || (second == third)) {
		g_warning (_("The 'first', 'second' and 'third' arguments must be different"));
		return FALSE;
	}
	if ((sep >= '0') && (sep <= '9')) {
		if (sep)
			g_warning (_("Invalid separator '%c'"), sep);
		else
			g_warning (_("Invalid null separator"));
		return FALSE;
	}

	/* 1st number */
	for (endptr = value, tmp = 0, iter = 0; (*endptr >= '0') && (*endptr <= '9') && (iter < 4); endptr++)
		tmp = tmp * 10 + *endptr - '0';
	parsed_values[0] = tmp;
	if (*endptr != sep)
		return FALSE;

	/* 2nd number */
	endptr++;
	for (tmp = 0, iter = 0; (*endptr >= '0') && (*endptr <= '9') && (iter < 4); endptr++)
		tmp = tmp * 10 + *endptr - '0';
	parsed_values[1] = tmp;
	if (*endptr != sep)
		return FALSE;

	/* 3rd number */
	endptr++;
	for (tmp = 0, iter = 0; (*endptr >= '0') && (*endptr <= '9') && (iter < 4); endptr++)
		tmp = tmp * 10 + *endptr - '0';
	parsed_values[2] = tmp;

	/* reordering */
	guint16 rmonth = 0, rday = 0;
	switch (first) {
	case G_DATE_YEAR:
		year = parsed_values[0];
		break;
	case G_DATE_MONTH:
		rmonth = parsed_values[0];
		break;
	case G_DATE_DAY:
		rday = parsed_values[0];
		break;
	default:
		g_warning (_("Unknown GDateDMY value %u"), first);
		return FALSE;
	}

	switch (second) {
	case G_DATE_YEAR:
		year = parsed_values[1];
		break;
	case G_DATE_MONTH:
		rmonth = parsed_values[1];
		break;
	case G_DATE_DAY:
		rday = parsed_values[1];
		break;
	default:
		g_warning (_("Unknown GDateDMY value %u"), second);
		return FALSE;
	}

	switch (third) {
	case G_DATE_YEAR:
		year = parsed_values[2];
		break;
	case G_DATE_MONTH:
		rmonth = parsed_values[2];
		break;
	case G_DATE_DAY:
		rday = parsed_values[2];
		break;
	default:
		g_warning (_("Unknown GDateDMY value %u"), third);
		return FALSE;
	}

	/* checks */
	month = rmonth > 0 ? (rmonth <= G_DATE_DECEMBER ? rmonth : G_DATE_BAD_MONTH) : G_DATE_BAD_MONTH;
	if (month == G_DATE_BAD_MONTH)
		return FALSE;
	day = rday > 0 ? (rday <= G_MAXUINT8 ? rday : G_DATE_BAD_DAY) : G_DATE_BAD_DAY;
	if (day == G_DATE_BAD_DAY)
		return FALSE;

	if (g_date_valid_dmy (day, month, year)) {
		g_date_set_dmy (gdate, day, month, year);
		if (out_endptr)
			*out_endptr = endptr;
		return TRUE;
	}
	else
		return FALSE;
}

static gboolean
_parse_iso8601_date (GDate *gdate, const gchar *value, const char **out_endptr)
{
	return _parse_formatted_date (gdate, value, G_DATE_YEAR, G_DATE_MONTH, G_DATE_DAY, '-', out_endptr);
}

/**
 * gda_parse_iso8601_date:
 * @gdate: a pointer to a #GDate structure which will be filled
 * @value: a string
 *
 * Extracts date parts from @value, and sets @gdate's contents
 *
 * Accepted date format is "YYYY-MM-DD" (more or less than 4 digits for years and
 * less than 2 digits for month and day are accepted). Years must be in the 1-65535 range,
 * a limitation imposed by #GDate.
 *
 * Returns: %TRUE if @value has been sucessfuly parsed as a valid date (see g_date_valid()).
 */
gboolean
gda_parse_iso8601_date (GDate *gdate, const gchar *value)
{
	g_return_val_if_fail (gdate, FALSE);

	const char *endptr;
	if (!value)
		return FALSE;

	if (! _parse_iso8601_date (gdate, value, &endptr) || *endptr)
		return FALSE;
	else
		return TRUE;
}

/**
 * gda_parse_formatted_date:
 * @gdate: a pointer to a #GDate structure which will be filled
 * @value: a string to be parsed
 * @first: a #GDateDMY specifying which of year, month or day appears first (in the first bytes) in @value
 * @second: a #GDateDMY specifying which of year, month or day appears second (in the first bytes) in @value
 * @third: a #GDateDMY specifying which of year, month or day appears third (in the first bytes) in @value
 * @sep: spcifies the expected separator character bewteen year, month and day (for example '-')
 *
 * This function is similar to gda_parse_iso8601_date() (with @first being @G_DATE_YEAR, @second being @G_DATE_MONTH,
 * @third being @G_DATE_DAY and @sep being '-') but allows one to specify the expected date format.
 *
 * Returns: %TRUE if @value has been sucessfuly parsed as a valid date (see g_date_valid()).
 *
 * Since: 5.2
 */
gboolean
gda_parse_formatted_date (GDate *gdate, const gchar *value, GDateDMY first, GDateDMY second, GDateDMY third, gchar sep)
{
	g_return_val_if_fail (gdate, FALSE);

	const char *endptr;
	if (!value)
		return FALSE;

	if (! _parse_formatted_date (gdate, value, first, second, third, sep, &endptr))
		return FALSE;
	if (*endptr)
		return FALSE;
	return TRUE;
}


static GdaTime *
_parse_iso8601_time (const gchar *value, gchar sep, glong timezone, const char **out_endptr)
{
	unsigned long int tmp;
	const char *endptr;
	gchar *stz = NULL;
	gdouble seconds = 0.0;
	int h, m;
	GTimeZone *tz;


	if ((*value < '0') || (*value > '9'))
		return NULL;

	/* hour */
	guint8 iter;
	for (iter = 0, tmp = 0, endptr = value; (*endptr >= '0') && (*endptr <= '9') && (iter < 2); iter++, endptr++) {
		tmp = tmp * 10 + *endptr - '0';
		if (tmp > 23)
			return NULL;
	}
	h = tmp;
	if ((sep && *endptr != sep) || !*endptr)
		return NULL;

	/* minutes */
	if (sep)
		endptr++;
	for (tmp = 0, iter = 0 ; (*endptr >= '0') && (*endptr <= '9') && (iter < 2); iter ++, endptr++) {
		tmp = tmp * 10 + *endptr - '0';
		if (tmp > 59)
			return NULL;
	}
	m = tmp;
	if ((sep && *endptr != sep) || !*endptr)
		return NULL;

	/* seconds */
	if (sep)
		endptr++;
	if (sep == 0 && (*endptr == ' ' || *endptr == ':'))
		return NULL;
	seconds = g_strtod ((const gchar*) endptr, &stz);
	if (seconds >= 60.0) {
		*out_endptr = stz;
		return NULL;
	}
	tz = g_time_zone_new_utc ();
	if (stz != NULL) {
		g_time_zone_unref (tz);
		tz = g_time_zone_new (stz);
		if (tz == NULL) {
			tz = g_time_zone_new_utc ();
		}
		for (; *endptr; endptr++);
	}
	GDateTime *dt = g_date_time_new (tz, 1978, 1, 1, h, m, seconds);

	g_time_zone_unref (tz);

	*out_endptr = endptr;
  if (dt != NULL) {
    return (GdaTime*)  dt;
  }

	return NULL;
}

/**
 * gda_parse_iso8601_time:
 * @value: a string
 *
 * Extracts time parts from @value, and sets @timegda's contents
 *
 * Accepted date format is "HH:MM:SS[.ms][TZ]" where TZ is +hour or -hour.
 * If no time zone is given UTC is used.
 *
 * Returns: (transfer full): a new parsed #GdaTime
 */
GdaTime *
gda_parse_iso8601_time (const gchar *value)
{
	g_return_val_if_fail (value != NULL, FALSE);
  gchar *str = g_strdup_printf ("1970-01-01T%s", value);
  GDateTime *dt;
  GTimeZone *tz = g_time_zone_new_utc ();
  dt = g_date_time_new_from_iso8601 (str, tz);
  g_time_zone_unref (tz);
  g_free (str);
  if (dt != NULL) {
    return (GdaTime*) dt;
  }
  return NULL;
}

/**
 * gda_parse_formatted_time:
 * @value: a string
 * @sep: the time separator, usually ':'. If equal to @0, then the expexted format will be HHMMSS...
 *
 * Returns: (transfer full): a new parsed #GdaTime
 *
 * Since: 6.0
 */
GdaTime *
gda_parse_formatted_time (const gchar *value, gchar sep)
{
	if (!value)
		return NULL;
	const char *endptr;
	return _parse_iso8601_time (value, sep, 0, &endptr);
}

/**
 * gda_parse_formatted_timestamp:
 * @value: a string to be parsed
 * @first: a #GDateDMY specifying which of year, month or day appears first (in the first bytes) in @value
 * @second: a #GDateDMY specifying which of year, month or day appears second (in the first bytes) in @value
 * @third: a #GDateDMY specifying which of year, month or day appears third (in the first bytes) in @value
 * @sep: specifies the expected separator character between year, month and day (for example '-')
 *
 * This function is similar to g_date_time_new_from_iso8601() (with @first being @G_DATE_YEAR, @second being @G_DATE_MONTH,
 * @third being @G_DATE_DAY and @sep being '-') but allows one to specify the expected date format.
 *
 * Returns: (nullable) (transfer full): a new #GDateTime if @value has been successfully parsed as a valid date (see g_date_valid()).
 * 
 * Since: 5.2
 */
GDateTime*
gda_parse_formatted_timestamp (const gchar *value,
			       GDateDMY first, GDateDMY second, GDateDMY third, gchar sep)
{
	g_return_val_if_fail (value != NULL, NULL);

	const char *endptr;
	GDate gdate;
	GdaTime* timegda = NULL;

	if (!value)
		return NULL;

	/* date part */
	if (! _parse_formatted_date (&gdate, value, first, second, third, sep, &endptr)) {
		return NULL;
	}
	if (endptr != NULL) {
		/* separator */
		if (*endptr != 'T' && *endptr != ' ') {
			return NULL;
		}
		value = endptr + 1;
		if (value != NULL) {
			/* time part */
      timegda = _parse_iso8601_time (value, ':', 0, &endptr);
			if (timegda == NULL  || *endptr)
				return NULL;
		}
	}

	gchar *stz;
	gint ts = gda_time_get_timezone (timegda);
	if (ts < 0) ts *= -1;
	gint h = ts/60/60;
	gint m = ts - h * 60;
	gint s = ts - h * 60 * 60 - m * 60;
	stz = g_strdup_printf ("%s%02d:%02d:%02d",
																gda_time_get_timezone (timegda) >= 0 ? "+" : "-",
																h, m, s);
  GTimeZone* tz = g_time_zone_new (stz);
	g_free (stz);
	if (tz == NULL)
		return NULL;
	gdouble seconds;
	seconds = (gdouble) gda_time_get_second (timegda) + gda_time_get_fraction (timegda) / 1000000;
	GDateTime *ret = g_date_time_new (tz,
													g_date_get_year (&gdate),
													g_date_get_month (&gdate),
													g_date_get_day (&gdate),
													gda_time_get_hour (timegda),
													gda_time_get_minute (timegda),
													seconds);
  gda_time_free (timegda);
  return ret;
}
