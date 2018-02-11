/*
 * Copyright (C) 2002 - 2005 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2003 Filip Van Raemdonck <mechanix@debian.org>
 * Copyright (C) 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2004 Julio M. Merino Vidal <jmmv@menta.net>
 * Copyright (C) 2004 Jürg Billeter <j@bitron.ch>
 * Copyright (C) 2005 - 2016 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2006 - 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 yselkowitz <yselkowitz@users.sourceforge.net>
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

#include <stdlib.h>
#include <glib.h>
#include <virtual/gda-vconnection-data-model.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-server-provider-impl.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-private.h>
#include <glib/gi18n-lib.h>
#include "gda-mdb.h"
#include "gda-mdb-provider.h"

#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-type.h>
#include <libgda/handlers/gda-handler-bin.h>

#define FILE_EXTENSION ".mdb"

static void gda_mdb_provider_class_init (GdaMdbProviderClass *klass);
static void gda_mdb_provider_init       (GdaMdbProvider *provider,
					 GdaMdbProviderClass *klass);
static void gda_mdb_provider_finalize   (GObject *object);

static const gchar *gda_mdb_provider_get_name (GdaServerProvider *provider);
static const gchar *gda_mdb_provider_get_version (GdaServerProvider *provider);
static gboolean gda_mdb_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc, GdaQuarkList *params, GdaQuarkList *auth);
static const gchar *gda_mdb_provider_get_server_version (GdaServerProvider *provider,
							 GdaConnection *cnc);

static GObjectClass *parent_class = NULL;
static GMutex mdb_init_mutex;
static gint loaded_providers = 0;
char *g_input_ptr;

/* 
 * private connection data destroy 
 */
static void gda_mdb_free_cnc_data (MdbConnectionData *cdata);

/*
 * GdaMdbProvider class implementation
 */
GdaServerProviderBase data_model_base_functions = {
        gda_mdb_provider_get_name,
        gda_mdb_provider_get_version,
        gda_mdb_provider_get_server_version,
        NULL,
        NULL,
	NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        gda_mdb_provider_prepare_connection,
        NULL,
        NULL,
        NULL,
	NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,

        NULL, NULL, NULL, NULL, /* padding */
};

static void
gda_mdb_provider_class_init (GdaMdbProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
                                                GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
                                                (gpointer) &data_model_base_functions);

	object_class->finalize = gda_mdb_provider_finalize;
}

static void
gda_mdb_provider_init (GdaMdbProvider *myprv, GdaMdbProviderClass *klass)
{
	mdb_set_date_fmt ("%Y-%m-%d %H:%M:%S");
}

static void
gda_mdb_provider_finalize (GObject *object)
{
	GdaMdbProvider *myprv = (GdaMdbProvider *) object;

	g_return_if_fail (GDA_IS_MDB_PROVIDER (myprv));

	/* chain to parent class */
	parent_class->finalize (object);

	/* call MDB exit function if there are no more providers */
	g_mutex_lock (&mdb_init_mutex);
	loaded_providers--;
	if (loaded_providers == 0)
		mdb_exit ();
	g_mutex_unlock (&mdb_init_mutex);
}

GType
gda_mdb_provider_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static GTypeInfo info = {
			sizeof (GdaMdbProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mdb_provider_class_init,
			NULL, NULL,
			sizeof (GdaMdbProvider),
			0,
			(GInstanceInitFunc) gda_mdb_provider_init,
			NULL
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_VPROVIDER_DATA_MODEL, "GdaMdbProvider", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}

GdaServerProvider *
gda_mdb_provider_new (void)
{
	GdaMdbProvider *provider;

	g_mutex_lock (&mdb_init_mutex);
	if (loaded_providers == 0) 
		mdb_init ();
	loaded_providers++;
	g_mutex_unlock (&mdb_init_mutex);

	provider = g_object_new (gda_mdb_provider_get_type (), NULL);
	return GDA_SERVER_PROVIDER (provider);
}

/*
 * Get provider name request
 */
static const gchar *
gda_mdb_provider_get_name (GdaServerProvider *provider)
{
	return MDB_PROVIDER_NAME;
}

/* 
 * Get version request
 */
static const gchar *
gda_mdb_provider_get_version (GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

static gchar *
sanitize_name (gchar *name) 
{
	gchar *ptr;
	gint len;
	
	len = strlen (name);
	if (g_utf8_validate (name, -1, NULL)) {
		
		for (ptr = name; ptr && *ptr; ptr = g_utf8_next_char (ptr), len--) {
			if (! g_unichar_isalnum (g_utf8_get_char (ptr))) {
				gchar *next = g_utf8_next_char (ptr);
				*ptr = '_';
				if (next != ptr+1) {
					memmove (ptr+1, next, len);
					len -= next - (ptr+1);
				}
			}
		}
	}
	else {
		/* for some reason @name is not UTF-8 */
		for (ptr = name; ptr && *ptr; ptr++) 
			*ptr = (isalnum (*ptr) ? *ptr : '_');
	}
	return name;
}


typedef struct {
        GdaVconnectionDataModelSpec spec;
	MdbCatalogEntry *table_entry;
	MdbConnectionData *cdata;
} LocalSpec;

static GList *table_create_columns_func (LocalSpec *spec);
static GdaDataModel *table_create_model_func (LocalSpec *spec);

/* 
 * Prapare connection request
 */
static gboolean
gda_mdb_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaQuarkList *params, G_GNUC_UNUSED GdaQuarkList *auth)
{
	gchar *filename = NULL, *tmp;
	const gchar *dirname = NULL, *dbname = NULL;
	gchar *dup = NULL;

	MdbConnectionData *cdata;
	GdaMdbProvider *mdb_prv = (GdaMdbProvider *) provider;

	g_return_val_if_fail (GDA_IS_MDB_PROVIDER (mdb_prv), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	/* look for parameters */
	dirname = gda_quark_list_find (params, "DB_DIR");
	if (!dirname)
		dirname= "."; /* default to current directory */
	dbname = gda_quark_list_find (params, "DB_NAME");
	
	if (!dbname) {
                const gchar *str;

                str = gda_quark_list_find (params, "FILENAME");
                if (!str) {
                        gda_connection_add_event_string (cnc,
                                                         _("The connection string must contain DB_DIR and DB_NAME values"));
                        return FALSE;
                }
                else {
                        gint len = strlen (str);
			gint elen = strlen (FILE_EXTENSION);
			dirname = NULL;

			if (g_str_has_suffix (str, FILE_EXTENSION)) {
                                gchar *ptr;

                                dup = g_strdup (str);
                                dup [len-elen] = 0;
                                for (ptr = dup + (len - elen - 1); (ptr >= dup) && (*ptr != G_DIR_SEPARATOR); ptr--);
                                dbname = ptr;
                                if (*ptr == G_DIR_SEPARATOR)
                                        dbname ++;

                                if ((*ptr == G_DIR_SEPARATOR) && (ptr > dup)) {
                                        dirname = dup;
                                        while ((ptr >= dup) && (*ptr != G_DIR_SEPARATOR))
                                                ptr--;
                                        *ptr = 0;
                                }
                        }
                        if (!dbname || !dirname) {
                                gda_connection_add_event_string (cnc,
                                                                 _("The connection string format has changed: replace FILENAME with "
                                                                   "DB_DIR (the path to the database file) and DB_NAME "
                                                                   "(the database file without the '%s' at the end)."), FILE_EXTENSION);
				g_free (dup);
                                return FALSE;
                        }
                        else
                                g_warning (_("The connection string format has changed: replace FILENAME with "
                                             "DB_DIR (the path to the database file) and DB_NAME "
                                             "(the database file without the '%s' at the end)."), FILE_EXTENSION);
                }
        }

	if (!g_file_test (dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
                gda_connection_add_event_string (cnc,
                                                 _("The DB_DIR part of the connection string must point "
                                                   "to a valid directory"));
                return FALSE;
        }

	tmp = g_strdup_printf ("%s%s", dbname, FILE_EXTENSION);
        filename = g_build_filename (dirname, tmp, NULL);
	g_free (dup);
	g_free (tmp);

	cdata = g_new0 (MdbConnectionData, 1);
	cdata->cnc = cnc;
	cdata->server_version = NULL;
#ifdef MDB_WITH_WRITE_SUPPORT
	cdata->mdb = mdb_open (filename, MDB_WRITABLE);
#else
	cdata->mdb = mdb_open (filename);
#endif
	if (!cdata->mdb) {
		gda_connection_add_event_string (cnc, _("Could not open file %s"), filename);
		gda_mdb_free_cnc_data (cdata);
		return FALSE;
	}

	mdb_read_catalog (cdata->mdb, MDB_ANY);
	gda_virtual_connection_internal_set_provider_data (GDA_VIRTUAL_CONNECTION (cnc), 
							   cdata, (GDestroyNotify) gda_mdb_free_cnc_data);

	/* declare the virtual tables */
	guint i;
	for (i = 0; i < cdata->mdb->num_catalog; i++) {
                MdbCatalogEntry *entry;

                entry = g_ptr_array_index (cdata->mdb->catalog, i);

                /* if it's a table */
                if (entry->object_type == MDB_TABLE) {
                        /* skip the MSys tables */
                        if (strncmp (entry->object_name, "MSys", 4)) {
                                /* table name is entry->object_name */
				LocalSpec *lspec;
				GError *error = NULL;
				gchar *tmp;
				lspec = g_new0 (LocalSpec, 1);
				GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->data_model = NULL;
				GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_columns_func = 
					(GdaVconnectionDataModelCreateColumnsFunc) table_create_columns_func;
				GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_model_func = 
					(GdaVconnectionDataModelCreateModelFunc) table_create_model_func;
				lspec->table_entry = entry;
				lspec->cdata = cdata;
				tmp = sanitize_name (g_strdup (entry->object_name));
				if (!gda_vconnection_data_model_add (GDA_VCONNECTION_DATA_MODEL (cnc), 
								     (GdaVconnectionDataModelSpec*) lspec,
								     g_free, tmp, &error)) {
					gda_connection_add_event_string (cnc, _("Could not map table '%s': %s"),
									 entry->object_name, 
									 error && error->message ? error->message : _("No detail"));
					g_free (lspec);
					g_error_free (error);
				}
				g_free (tmp);
                        }
                }
        }

	return TRUE;
}

static GType
gda_mdb_type_to_gda (int col_type)
{
        switch (col_type) {
        case MDB_BOOL : return G_TYPE_BOOLEAN;
        case MDB_BYTE : return G_TYPE_CHAR;
        case MDB_DOUBLE : return G_TYPE_DOUBLE;
        case MDB_FLOAT : return G_TYPE_FLOAT;
        case MDB_INT : return G_TYPE_INT;
        case MDB_LONGINT : return G_TYPE_INT64;
        case MDB_MEMO : return G_TYPE_STRING;
        case MDB_MONEY : return G_TYPE_DOUBLE;
        case MDB_NUMERIC : return GDA_TYPE_NUMERIC;
        case MDB_OLE : return GDA_TYPE_BINARY;
        case MDB_REPID : return GDA_TYPE_BINARY;
#ifdef MDB_V07
        case MDB_DATETIME : return GDA_TYPE_TIMESTAMP;
#else
        case MDB_SDATETIME : return GDA_TYPE_TIMESTAMP;
#endif
        case MDB_TEXT : return G_TYPE_STRING;
        }

        return G_TYPE_INVALID;
}

static GList *
table_create_columns_func (LocalSpec *spec)
{
	guint j;
        GList *columns = NULL;
	MdbTableDef *mdb_table;
	
	mdb_table = mdb_read_table (spec->table_entry);
	mdb_read_columns (mdb_table);

	for (j = 0; j < mdb_table->num_cols; j++) {
		MdbColumn *mdb_col;
		GdaColumn *gda_col;
		gchar *tmp;

		gda_col = gda_column_new ();
		mdb_col = g_ptr_array_index (mdb_table->columns, j);
		
		tmp = sanitize_name (g_strdup (mdb_col->name));
		gda_column_set_name (gda_col, tmp);
		g_free (tmp);
		gda_column_set_g_type (gda_col, gda_mdb_type_to_gda (mdb_col->col_type));
#ifdef MDB_V07
		tmp = sanitize_name (g_strdup (mdb_get_colbacktype_string (mdb_col)));
#else
		tmp = sanitize_name (g_strdup (mdb_get_coltype_string (spec->cdata->mdb->default_backend, 
								       mdb_col->col_type)));
#endif
		gda_column_set_dbms_type (gda_col, tmp);
		g_free (tmp);
		columns = g_list_prepend (columns, gda_col);
	}

	return g_list_reverse (columns);
}

static GdaDataModel *
table_create_model_func (LocalSpec *spec)
{
	GdaDataModel *model;
	MdbTableDef *mdb_table;
	GType *coltypes;
	guint c;

	char **bound_values;
	int *bound_len;

	mdb_table = mdb_read_table (spec->table_entry);
	mdb_read_columns (mdb_table);
	mdb_rewind_table (mdb_table);

	/* prepare data model */
	/*g_print ("New data model for MDB table %p\n", mdb_table);*/
	model = gda_data_model_array_new (mdb_table->num_cols);
	
	/* prepare column types */
	bound_values = g_new0 (gchar *, mdb_table->num_cols);
	bound_len = g_new0 (int, mdb_table->num_cols);
	coltypes = g_new0 (GType, mdb_table->num_cols);
        for (c = 0; c < mdb_table->num_cols; c++) {
                MdbColumn *mdb_col;
		GdaColumn *gda_col;
		gchar *tmp;

                /* column type */
                mdb_col = g_ptr_array_index (mdb_table->columns, c);
                coltypes [c] = gda_mdb_type_to_gda (mdb_col->col_type);

                /* allocate bound data */
		bound_values[c] = (char *) malloc (MDB_BIND_SIZE);
		bound_values[c][0] = '\0';

#ifdef MDB_BIND_COLUMN_FOUR_ARGS
                mdb_bind_column (mdb_table, c + 1, bound_values[c], &(bound_len[c]));
#else
                mdb_bind_column (mdb_table, c + 1, bound_values[c]);
		mdb_bind_len (mdb_table, c + 1, &(bound_len[c]));
#endif
		/* column's name */
		gda_col = gda_data_model_describe_column (model, c);
		tmp = sanitize_name (g_strdup (mdb_col->name));
		gda_column_set_name (gda_col, tmp);
		gda_column_set_description (gda_col, tmp);
		g_free (tmp);
#ifdef MDB_V07
		tmp = sanitize_name (g_strdup (mdb_get_colbacktype_string (mdb_col)));

#else
		tmp = sanitize_name (g_strdup (mdb_get_coltype_string (spec->cdata->mdb->default_backend,
								       mdb_col->col_type)));
#endif
		gda_column_set_dbms_type (gda_col, tmp);
		g_free (tmp);
                gda_column_set_g_type (gda_col, coltypes [c]);
#ifdef GDA_DEBUG_NO
		g_print ("col: %s (%s/%s)\n", gda_column_get_name (gda_col), gda_column_get_dbms_type (gda_col),
			 g_type_name (coltypes [c]));
#endif
        }

	/* read data */
	while (mdb_fetch_row (mdb_table)) {
		MdbColumn *mdb_col;
		GList *value_list = NULL;
		GValue *tmpval;

		for (c = 0; c < mdb_table->num_cols; c++) {
			mdb_col = g_ptr_array_index (mdb_table->columns, c);
			if (mdb_col->col_type == MDB_OLE) {
				tmpval = gda_value_new_binary ((guchar*) bound_values[c],
							       mdb_ole_read (spec->cdata->mdb, mdb_col, bound_values[c],
									     MDB_BIND_SIZE));
#ifdef DUMP_BINARY
				{
					static int index = 0;
					gchar *file = g_strdup_printf ("OLE_%d.bin", index++);
					GdaBinary *bin;
					bin = gda_value_get_binary (tmpval);
					g_file_set_contents (file, gda_binary_get_data (bin), gda_binary_get_size (bin), NULL);
					g_free (file);
				}
#endif
			}
			else
				gda_value_set_from_string ((tmpval = gda_value_new (coltypes [c])), bound_values[c], 
							   coltypes [c]);
                        value_list = g_list_append (value_list, tmpval);
		}

		gda_data_model_append_values (GDA_DATA_MODEL (model), value_list, NULL);
		g_list_foreach (value_list, (GFunc) gda_value_free, NULL);
                g_list_free (value_list);
	}

        /* free memory */
        g_free (coltypes);
        for (c = 0; c < mdb_table->num_cols; c++)
                free (bound_values [c]);
	g_free (bound_values);
	g_free (bound_len);

	g_object_set (G_OBJECT (model), "read-only", TRUE, NULL);
	return model;
}

/*
 * Server version request
 */
static const gchar *
gda_mdb_provider_get_server_version (GdaServerProvider *provider,
				     GdaConnection *cnc)
{
	MdbConnectionData *cdata;
	
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	cdata = gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
	if (!cdata)
		return NULL;

	if (!cdata->server_version)
		cdata->server_version = g_strdup_printf ("Microsoft Jet %d", cdata->mdb->f->jet_version);

	return (const gchar *) cdata->server_version;
}

/*
 * Free connection's specific data
 */
static void
gda_mdb_free_cnc_data (MdbConnectionData *cdata)
{
	if (cdata->mdb)
		mdb_close (cdata->mdb);
	g_free (cdata->server_version);
	g_free (cdata);
}
