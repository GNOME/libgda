/* gda-db-catalog.c
 *
 * Copyright (C) 2018-2019 Pavlo Solntsev <p.sun.fun@gmail.com>
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

#define G_LOG_DOMAIN "GDA-db-catalog"

#include "gda-db-catalog.h"
#include "gda-db-table.h"
#include "gda-db-view.h"
#include "gda-db-fkey.h"
#include "gda-db-column-private.h"
#include "gda-db-fkey-private.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n-lib.h>
#include <libgda/binreloc/gda-binreloc.h>

#include "gda-server-provider.h"
#include <libgda/gda-lockable.h>
#include "gda-meta-struct.h"
#include "gda-ddl-modifiable.h"

G_DEFINE_QUARK (gda_db_catalog_error, gda_db_catalog_error)

typedef struct
{
  GList *mp_tables; /* List of all tables that should be create, GdaDbTable  */
  GList *mp_views; /* List of all views that should be created, GdaDbView */
  gchar *mp_schemaname;
  GdaConnection *cnc;
} GdaDbCatalogPrivate;

/**
 * SECTION:gda-db-catalog
 * @title: GdaDbCatalog
 * @short_description: Object to constract database representation from an xml file or by reading
 * the existing datatabase
 * @see_also: #GdaDbTable, #GdaDbView
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This is a main object that represents overall database. The best way to create an instance of
 * #GdaDbCatalog is to call gda_connection_create_db_catalog(). This object may be also constracted
 * from an xml file using gda_db_catalog_parse_file() or gda_db_catalog_parse_file_from_path() or
 * didrectly from the open connection using gda_db_catalog_parse_cnc().
 *
 * The database can be updated or modified using gda_db_catalog_perform_operation() and dumped to
 * a file using gda_db_catalog_write_to_path() or gda_db_catalog_write_to_file().
 *
 */

G_DEFINE_TYPE_WITH_PRIVATE (GdaDbCatalog, gda_db_catalog, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SCHEMA_NAME,
  PROP_CNC,
  /*<private>*/
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = {NULL};

/**
 * gda_db_catalog_new:
 *
 * Create new instance of #GdaDbCatalog.
 *
 * Returns: a new instance of #GdaDbCatalog
 *
 * Since: 6.0
 */
GdaDbCatalog*
gda_db_catalog_new (void)
{
  return g_object_new (GDA_TYPE_DB_CATALOG, NULL);
}

static void
gda_db_catalog_finalize (GObject *object)
{
  GdaDbCatalog *self = GDA_DB_CATALOG(object);
  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  g_free (priv->mp_schemaname);

  G_OBJECT_CLASS (gda_db_catalog_parent_class)->finalize (object);
}

static void
gda_db_catalog_dispose (GObject *object)
{
  GdaDbCatalog *self = GDA_DB_CATALOG(object);
  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  if (priv->mp_tables)
    g_list_free_full (priv->mp_tables, (GDestroyNotify) g_object_unref);

  if (priv->mp_views)
    g_list_free_full (priv->mp_views, (GDestroyNotify) g_object_unref);

  if (priv->cnc)
    g_object_unref (priv->cnc);

  G_OBJECT_CLASS (gda_db_catalog_parent_class)->dispose (object);
}

static void
gda_db_catalog_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GdaDbCatalog *self = GDA_DB_CATALOG (object);
  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  switch (prop_id)
    {
      case PROP_SCHEMA_NAME:
        g_value_set_string (value, priv->mp_schemaname);
      break;
      case PROP_CNC:
        g_value_set_object (value, G_OBJECT(priv->cnc));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gda_db_catalog_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GdaDbCatalog *self = GDA_DB_CATALOG (object);
  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  switch (prop_id)
    {
      case PROP_SCHEMA_NAME:
        g_free(priv->mp_schemaname);
        priv->mp_schemaname = g_value_dup_string (value);
      break;
      case PROP_CNC:
        if (priv->cnc)
          g_object_unref (priv->cnc);

        priv->cnc = GDA_CONNECTION (g_object_ref (G_OBJECT (g_value_get_object (value))));
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gda_db_catalog_class_init (GdaDbCatalogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_db_catalog_finalize;
  object_class->dispose = gda_db_catalog_dispose;
  object_class->get_property = gda_db_catalog_get_property;
  object_class->set_property = gda_db_catalog_set_property;

  properties[PROP_SCHEMA_NAME] =
    g_param_spec_string ("schema_name",
                         "schema-name",
                         "Name of the schema",
                         NULL,
                         G_PARAM_READWRITE);

  properties[PROP_CNC] =
    g_param_spec_object ("connection",
                         "Connection",
                         "Connection to use",
                         GDA_TYPE_CONNECTION,
                         G_PARAM_READWRITE);

  g_object_class_install_properties (object_class,N_PROPS,properties);
}

static void
gda_db_catalog_init (GdaDbCatalog *self)
{
  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  priv->mp_tables = NULL;
  priv->mp_views = NULL;
  priv->mp_schemaname = NULL;
  priv->cnc = NULL;
}

xmlDtdPtr
_gda_db_catalog_get_dtd ()
{
  xmlDtdPtr		_gda_db_catalog_dtd = NULL;
  gchar *file;
  /* GdaDbCreator DTD */
	file = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "dtd",
                                "libgda-db-catalog.dtd", NULL);
	if (g_file_test (file, G_FILE_TEST_EXISTS)) {
		_gda_db_catalog_dtd = xmlParseDTD (NULL, (xmlChar*)file);
  } else {
      const gchar *gdatopsrcdir = g_getenv ("GDA_TOP_SRC_DIR");
	    if (gdatopsrcdir)
        {
          g_free (file);
          file = g_build_filename (gdatopsrcdir, "libgda",
                                   "libgda-db-catalog.dtd", NULL);
          _gda_db_catalog_dtd = xmlParseDTD (NULL, (xmlChar*)file);
		    }
	  }

  if (_gda_db_catalog_dtd != NULL) {
    _gda_db_catalog_dtd->name = xmlStrdup((xmlChar*) "db-catalog");
  } else {
	  g_message (_("Could not parse '%s': "
                 "Validation for XML files for GdaDbCreator will not be performed (some weird errors may occur)"),
               file);
  }

	g_free (file);
  return _gda_db_catalog_dtd;
}

static gboolean
_gda_db_catalog_validate_doc (xmlDocPtr doc,
                              GError **error)
{
  g_return_val_if_fail (doc, FALSE);

  xmlValidCtxtPtr  ctx = NULL;

  ctx = xmlNewValidCtxt ();

  if (!ctx)
    {
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_CONTEXT_NULL,
                   _("Context to validate xml file can't be created."));
      goto on_error;
    }

  xmlDtdPtr _gda_db_catalog_dtd = _gda_db_catalog_get_dtd ();

  int valid = 0;

  if (_gda_db_catalog_dtd != NULL) {
    valid = xmlValidateDtd (ctx, doc,_gda_db_catalog_dtd);
    xmlFreeDtd (_gda_db_catalog_dtd);
  }

  if (!valid)
    {
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_INVALID_XML,
                   _("xml file is invalid"));
      goto on_error;
    }

  xmlFreeValidCtxt (ctx);
  return TRUE;

on_error:
  if (ctx)  xmlFreeValidCtxt (ctx);
  return FALSE;
}

/**
 * _gda_db_catalog_parse_doc:
 * @self: a #GdaDbCatalog instance
 * @doc: a #xmlDocPtr instance to parse
 * @error: error container
 *
 * Internal method to populate #GdaDbCatalog from @doc instance.
 *
 * Returns: %TRUE if no errors, %FALSE otherwise.
 *
 */
static gboolean
_gda_db_catalog_parse_doc (GdaDbCatalog *self,
                           xmlDocPtr doc,
                           GError **error)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (doc, FALSE);

  xmlChar         *schema_name = NULL;
  xmlNodePtr       node        = NULL;

  node = xmlDocGetRootElement (doc);

  if (!node || g_strcmp0 ((gchar *)node->name, "schema") != 0)
    {
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_INVALID_SCHEMA,
                   _("Root node should be <schema>."));
      goto on_error;
    }

  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  schema_name = xmlGetProp(node,(xmlChar *)"name");
//  g_assert (schema_name); /* If schema_name == NULL we have problem with validation */
  if (schema_name)
    g_object_set (self, "schema_name", (char*)schema_name, NULL);

  xmlFree (schema_name);

  /* walk through the xmlDoc */
  for (node = node->children; node; node = node->next)
    {
    /* <table> tag for table creation */
    if (!g_strcmp0 ((gchar *) node->name, "table"))
      {
        GdaDbTable *table;
        table = gda_db_table_new ();
        gda_db_base_set_schema (GDA_DB_BASE(table), priv->mp_schemaname);

        if (!gda_db_buildable_parse_node (GDA_DB_BUILDABLE(table), node, error))
          {
            g_object_unref (table);
            goto on_error;
          }
        else
            priv->mp_tables = g_list_append (priv->mp_tables,table);
      }
    else if (!g_strcmp0 ((gchar *) node->name, "view"))
      {
        GdaDbView *view;
        view = gda_db_view_new ();
        gda_db_base_set_schema (GDA_DB_BASE(view), priv->mp_schemaname);

        if (!gda_db_buildable_parse_node (GDA_DB_BUILDABLE(view), node, error))
          {
            g_object_unref (view);
            goto on_error;
          }
        else
          priv->mp_views = g_list_append (priv->mp_views, view);
      }  /* end of else if */
  } /* End of for loop */

  xmlFree (node);
  return TRUE;

on_error:
  if (node) xmlFree (node);
  return FALSE;
}

/**
 * gda_db_catalog_parse_file_from_path:
 * @self: an instance of #GdaDbCatalog
 * @xmlfile: xml file to parse
 * @error: Error container
 *
 * This method reads information from @xmlfile and populate @self object.
 * The @xmlfile should correspond to the following DTD format:
 *
 * |[<!-- language="DTD" -->
 * <!ELEMENT schema (table+, view*)>
 * <!ATTLIST schema name           CDATA   #IMPLIED>
 *
 * <!ELEMENT table (comment?,column+, fkey*, constraint*)>
 * <!ATTLIST table temptable       (TRUE|FALSE)    "FALSE">
 * <!ATTLIST table name            CDATA           #REQUIRED>
 * <!ATTLIST table space           CDATA           #IMPLIED>
 *
 * <!ELEMENT column (comment?, value?, check?)>
 * <!ATTLIST column name           CDATA           #REQUIRED>
 * <!ATTLIST column type           CDATA           #REQUIRED>
 * <!ATTLIST column pkey           (TRUE|FALSE)    "FALSE">
 * <!ATTLIST column unique         (TRUE|FALSE)    "FALSE">
 * <!ATTLIST column autoinc        (TRUE|FALSE)    "FALSE">
 * <!ATTLIST column nnul           (TRUE|FALSE)    "FALSE">
 *
 * <!ELEMENT comment       (#PCDATA)>
 * <!ELEMENT value         (#PCDATA)>
 * <!ATTLIST value size            CDATA          #IMPLIED>
 * <!ATTLIST value scale           CDATA          #IMPLIED>
 *
 * <!ELEMENT check         (#PCDATA)>
 *
 * <!ELEMENT constraint    (#PCDATA)>
 *
 * <!ELEMENT fkey (fk_field?)>
 * <!ATTLIST fkey reftable CDATA #IMPLIED>
 * <!ATTLIST fkey onupdate (RESTRICT|CASCADE|SET_NULL|NO_ACTION|SET_DEFAULT)       #IMPLIED>
 * <!ATTLIST fkey ondelete (RESTRICT|CASCADE|SET_NULL|NO_ACTION|SET_DEFAULT)       #IMPLIED>
 *
 * <!ELEMENT fk_field (#PCDATA)>
 * <!ATTLIST fk_field name         CDATA           #REQUIRED>
 * <!ATTLIST fk_field reffield     CDATA           #REQUIRED>
 *
 * <!ELEMENT view (definition)>
 * <!ATTLIST view name             CDATA           #REQUIRED>
 * <!ATTLIST view replace          (TRUE|FALSE)    "FALSE">
 * <!ATTLIST view temp             (TRUE|FALSE)    "FALSE">
 * <!ATTLIST view ifnotexists      (TRUE|FALSE)    "TRUE">
 *
 * <!ELEMENT definition (#PCDATA)>
 * ]|
 *
 * Up to day description of the xml file schema can be found in DTD file
 * [libgda-db-catalog.dtd](https://gitlab.gnome.org/GNOME/libgda/blob/master/libgda/libgda-db-catalog.dtd)
 *
 * The given @xmlfile will be checked before parsing and %FALSE will be
 * returned if fails. The @xmlfile will be validated internally using
 * gda_db_catalog_validate_file_from_path(). he same method can be used to validate xmlfile
 * before parsing it.
 */
gboolean
gda_db_catalog_parse_file_from_path (GdaDbCatalog *self,
                                     const gchar *xmlfile,
                                     GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (xmlfile,FALSE);

  xmlDocPtr doc = NULL;

  doc = xmlParseFile (xmlfile);

  if (!doc)
    {
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_DOC_NULL,
                   _("xmlDoc object can't be created from xmfile name '%s'"), xmlfile);
      goto on_error;
    }

  if (!_gda_db_catalog_validate_doc (doc, error))
    {
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_INVALID_XML,
                   _("xml file '%s' is not valid\n"), xmlfile);
      goto on_error;
    }

  if (!_gda_db_catalog_parse_doc (self, doc, error))
    goto on_error;

  xmlFreeDoc (doc);
  return TRUE;

on_error:
  if (doc) xmlFreeDoc (doc);

  return FALSE;
}

/**
 * gda_db_catalog_validate_file_from_path:
 * @xmlfile: xml file to validate
 * @error: error container
 *
 * Convenient method to varify xmlfile before prsing it.
 *
 * Returns: %TRUE if @xmlfile is valid, %FALSE otherwise
 *
 * Since: 6.0
 */
gboolean
gda_db_catalog_validate_file_from_path (const gchar *xmlfile,
                                        GError **error)
{
  g_return_val_if_fail (xmlfile, FALSE);

  xmlDocPtr doc = NULL;

  doc = xmlParseFile(xmlfile);

  if (!doc)
    {
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_DOC_NULL,
                   _("xmlDoc object can't be created from xmfile name '%s'"), xmlfile);
      goto on_error;
    }

  if (!_gda_db_catalog_validate_doc (doc, error))
    goto on_error;

  xmlFreeDoc (doc);

  return TRUE;

on_error:
  if (doc)  xmlFreeDoc (doc);
  return FALSE;
}

/**
 * gda_db_catalog_get_tables:
 * @self: a #GdaDbCatalog object
 *
 * Returns: (element-type Gda.DbTable) (transfer none): a list of tables as #GdaDbTable or %NULL.
 *
 * Since: 6.0
 */
GList*
gda_db_catalog_get_tables (GdaDbCatalog *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);
  return priv->mp_tables;
}

/**
 * gda_db_catalog_get_views:
 * @self: a #GdaDbCatalog object
 *
 * Returns: (element-type Gda.DbView) (transfer none): a list of views as #GdaDbView or %NULL
 *
 * Since: 6.0
 */
GList*
gda_db_catalog_get_views (GdaDbCatalog *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);
  return priv->mp_views;
}

/**
 * gda_db_catalog_get_table:
 * @self: a #GdaDbCatalog object
 * @name: table name
 *
 * Convenient function to return a table based on name
 *
 * Returns: table as #GdaDbTable or %NULL
 *
 */
const GdaDbTable*
gda_db_catalog_get_table (GdaDbCatalog *self,
                          const gchar *catalog,
                          const gchar *schema,
                          const gchar *name)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (name, NULL);

  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  GdaDbBase *iobj = gda_db_base_new ();
  gda_db_base_set_names (iobj, catalog, schema, name);

  GList *it = NULL;

  for (it = priv->mp_tables; it; it = it->next)
      if (!gda_db_base_compare (iobj,GDA_DB_BASE(it->data)))
        {
          g_object_unref (iobj);
          return GDA_DB_TABLE(it->data);
        }

  g_object_unref (iobj);
  return NULL;
}

/**
 * gda_db_catalog_get_view:
 * @self: a #GdaDbCatalog object
 * @name: view name
 *
 * Convenient function to return a view based on name.
 *
 * Returns: table as #GdaDbView or %NULL if the view is not found.
 *
 */
const GdaDbView*
gda_db_catalog_get_view (GdaDbCatalog *self,
                         const gchar *catalog,
                         const gchar *schema,
                         const gchar *name)
{
  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  GList *it = NULL;

  GdaDbBase *iobj = gda_db_base_new ();
  gda_db_base_set_names (iobj,catalog,schema,name);

  for (it = priv->mp_views; it; it = it->next)
    if (!gda_db_base_compare (iobj, GDA_DB_BASE(it)))
      {
        g_object_unref (iobj);
        return GDA_DB_VIEW(it);
      }

  g_object_unref (iobj);
  return NULL;
}

/*
 * _gda_db_table_new_from_meta:
 * @obj: a #GdaMetaDbObject
 *
 * Create new #GdaDbTable instance from the corresponding #GdaMetaDbObject
 * object. If %NULL is passed this function works exactly as
 * gda_db_table_new()
 *
 * This method should not be part of the public API.
 *
 * Since: 6.0
 */
static GdaDbTable*
_gda_db_table_new_from_meta (GdaMetaDbObject *obj)
{
  if (!obj)
    return gda_db_table_new ();

  GdaMetaTable *metatable = GDA_META_TABLE(obj);
  GdaDbTable *table = gda_db_table_new ();

  gda_db_base_set_names(GDA_DB_BASE(table),
                        obj->obj_catalog,
                        obj->obj_schema,
                        obj->obj_name);

  GSList *it = NULL;
  for (it = metatable->columns; it; it = it->next)
    {
      GdaDbColumn *column = gda_db_column_new_from_meta (GDA_META_TABLE_COLUMN(it->data));

      gda_db_table_append_column (table, column);
      g_object_unref (column);
    }

  for (it = metatable->fk_list;it;it = it->next)
    {
      if (!GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED(GDA_META_TABLE_FOREIGN_KEY(it->data)))
        continue;

      GdaDbFkey *fkey = gda_db_fkey_new_from_meta (GDA_META_TABLE_FOREIGN_KEY(it->data));

      gda_db_table_append_fkey (table, fkey);
      g_object_unref (fkey);

    }

  return table;
}

/*
 * _gda_db_view_new_from_meta:
 * @view: a #GdaMetaView instance
 *
 * Create new #GdaDbView object from the corresponding #GdaMetaView object
 *
 * This method should not be part of the public API.
 *
 * Returns: New instance of #GdaDbView
 */
static GdaDbView*
_gda_db_view_new_from_meta (GdaMetaView *view)
{
  if (!view)
    return gda_db_view_new ();

  GdaDbView *dbview = gda_db_view_new ();

  gda_db_view_set_defstring (dbview,view->view_def);
  gda_db_view_set_replace (dbview,view->is_updatable);

  return dbview;
}

/**
 * gda_db_catalog_parse_cnc:
 * @self: a #GdaDbCatalog instance
 * @error: error storage object
 *
 * Parse internal cnc to populate @self object. This method should be called every time after
 * database was modified or @self was just created using gda_connection_create_db_catalog(). The
 * method will return %FALSE if no internal #GdaConnection available.
 *
 * Returns: Returns %TRUE if succeeded, %FALSE otherwise.
 *
 * Since: 6.0
 */
gboolean
gda_db_catalog_parse_cnc (GdaDbCatalog *self,
                          GError **error)
{
  g_return_val_if_fail (self, FALSE);

  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  if (!priv->cnc && !gda_connection_is_opened (priv->cnc))
    return FALSE;

  if(!gda_connection_update_meta_store (priv->cnc, NULL, error))
    return FALSE;


  GdaMetaStore *mstore = NULL;
  GdaMetaStruct *mstruct = NULL;

  mstore = gda_connection_get_meta_store (priv->cnc);
  mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT,
                                           "meta-store", mstore,
                                           "features", GDA_META_STRUCT_FEATURE_ALL, NULL);

  GSList *dblist = NULL;

  if(!gda_meta_struct_complement_all (mstruct, error))
    goto on_error;

  dblist = gda_meta_struct_get_all_db_objects (mstruct);

  if (!dblist)
    goto on_error;

  GSList *it = NULL;

  for (it=dblist; it; it = it->next)
    {
      if(GDA_META_DB_OBJECT(it->data)->obj_type == GDA_META_DB_TABLE)
        {
          GdaDbTable *table = _gda_db_table_new_from_meta (it->data);
	  priv->mp_tables = g_list_append (priv->mp_tables,table);
          continue;
        }

      if(GDA_META_DB_OBJECT(it->data)->obj_type == GDA_META_DB_VIEW)
        {
          GdaDbView *view = _gda_db_view_new_from_meta (it->data);
          priv->mp_views = g_list_append (priv->mp_views, view);
          continue;
        }
    }

  g_slist_free (dblist);
  g_object_unref (mstruct);
  return TRUE;

on_error:
  if (dblist)
    g_slist_free (dblist);
  g_object_unref (mstruct);
  return FALSE;
}

/**
 * gda_db_catalog_append_table:
 * @self: a #GdaDbCatalog instance
 * @table: table to append
 *
 * This method append @table to the total list of all tables stored in @self. This method increase
 * reference count for @table.
 *
 * Since: 6.0
 */
void
gda_db_catalog_append_table (GdaDbCatalog *self,
                             GdaDbTable *table)
{
  g_return_if_fail (self);

  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  priv->mp_tables = g_list_append (priv->mp_tables, g_object_ref (table));
}

/**
 * gda_db_catalog_append_view:
 * @self: a #GdaDbCatalog instance
 * @view: view to append
 *
 * This method append @view to the total list of all views stored in @self. This method increase
 * reference count for @view.
 *
 * Since: 6.0
 */
void
gda_db_catalog_append_view (GdaDbCatalog *self,
                            GdaDbView *view)
{
  g_return_if_fail (self);

  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  priv->mp_views = g_list_append (priv->mp_views, g_object_ref (view));
}

/**
 * gda_db_catalog_perform_operation:
 * @self: a #GdaDbCatalog object
 * @error: object to store error
 *
 * After population @self with all data this method may be
 * called to trigger code and modify user database. This is the main
 * method to work with database. For retrieving information from database to an
 * xml file use gda_db_catalog_parse_cnc() and gda_db_buildable_write_node().
 *
 * Connection can be added as a property using g_object_set() call and should be opened to use
 * this method. See gda_connection_open() method for reference.
 *
 * Only table can be created. Views are ignored
 *
 * Each table from database compared with each table in the #GdaDbCatalog
 * instance. If the table doesn't exist in database, it will be created (CREATE_TABLE).
 * If table exists in the database and xml file, all columns will be checked. If columns
 * are present in xml file but not in the database it will be created (ADD_COLUMN). If
 * column exists but has different parameters, e.g. nonnull it will not be
 * modified.
 *
 * Note: Pkeys are not checked. This is a limitation that should be removed. The corresponding
 * issue was open on gitlab page.
 */
gboolean
gda_db_catalog_perform_operation (GdaDbCatalog *self,
                                  GError **error)
{
  g_return_val_if_fail (self, FALSE);
  gboolean st = TRUE;

  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  if (!gda_connection_is_opened (priv->cnc))
    {
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_CONNECTION_CLOSED,
                   _("Connection is not opened"));
      return FALSE;
    }

  gda_lockable_lock ((GdaLockable*)priv->cnc);
// We need to get MetaData
  if(!gda_connection_update_meta_store (priv->cnc, NULL, error))
    return FALSE;

  GdaMetaStore *mstore = gda_connection_get_meta_store (priv->cnc);
  GdaMetaStruct *mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT,
                                                          "meta-store", mstore,
                                                          "features", GDA_META_STRUCT_FEATURE_ALL,
                                                          NULL);

  // We need information about catalog, schema, name for each object we would
  // like to check

  GdaMetaDbObject *mobj = NULL;
  GValue *catalog = NULL;
  GValue *schema = NULL;
  GValue *name = NULL;

  catalog = gda_value_new (G_TYPE_STRING);
  schema  = gda_value_new (G_TYPE_STRING);
  name    = gda_value_new (G_TYPE_STRING);

  GList *it = NULL;
  for (it = priv->mp_tables; it; it = it->next)
    {
      /* SQLite accepts only one possible value for schema: "main".
       * Other databases may also ignore schema and reference objects only by database and
       * name, e.g. db_name.table_name
       * */
      g_value_set_string (catalog, gda_db_base_get_catalog(it->data));
      g_value_set_string (schema , gda_db_base_get_schema (it->data));
      g_value_set_string (name   , gda_db_base_get_name   (it->data));

      mobj = gda_meta_struct_complement (mstruct, GDA_META_DB_TABLE, catalog, schema,name, NULL);

      if (mobj)
				{
					g_debug("Object %s was found in the database\n", gda_db_base_get_name(GDA_DB_BASE(it->data)));
					st = gda_db_table_update (it->data,GDA_META_TABLE(mobj), priv->cnc, error);

					if (!st)
						break;
				}
			else
				{
					st = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (it->data), priv->cnc, NULL, error);

					if (!st)
						break;
				}
    } /* End of for loop */

  gda_value_free (catalog);
  gda_value_free (schema);
  gda_value_free (name);

  if (st) {
  /*TODO: add update option for views */
    for (it = priv->mp_views; it; it = it->next) {
        st = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (it->data), priv->cnc, NULL, error);
        if (!st) {
          break;
        }
    } /* End of for loop */
  }

  g_object_unref (mstruct);
  gda_lockable_unlock ((GdaLockable*) priv->cnc);

  return st;
}

/**
 * gda_db_catalog_write_to_file:
 * @self: a #GdaDbCatalog instance
 * @file: a #GFile to write database description
 * @error: container to hold error
 *
 * This method writes database description as xml file.
 * Similar to gda_db_catalog_write_to_path()
 *
 * Returns: %TRUE if no error occurred, %FALSE otherwise.
 *
 * Since: 6.0
 */
gboolean
gda_db_catalog_write_to_file (GdaDbCatalog *self,
                              GFile *file,
                              GError **error)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (file, FALSE);

  gchar *filepath = g_file_get_path (file);
  gboolean res = gda_db_catalog_write_to_path (self, filepath, error);
  g_free (filepath);
  return res;
}

/**
 * gda_db_catalog_write_to_path:
 * @self: a #GdaDbCatalog instance
 * @path: path to xml file to save #GdaDbCatalog
 * @error: container to hold an error
 *
 * Save content of @self to a user friendly xml file.
 *
 * Returns: %TRUE is no error, %FALSE otherwise.
 *
 * Since: 6.0
 */
gboolean
gda_db_catalog_write_to_path (GdaDbCatalog *self,
                              const gchar *path,
                              GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (path,FALSE);

  GdaDbCatalogPrivate *priv = gda_db_catalog_get_instance_private (self);

  xmlDocPtr doc = NULL;
  xmlNodePtr node_root = NULL;

  doc = xmlNewDoc (BAD_CAST "1.0");
  node_root = xmlNewNode (NULL, BAD_CAST "schema");

  if (priv->mp_schemaname)
    xmlNewProp (node_root, (xmlChar*)"name",(xmlChar*)priv->mp_schemaname);

  xmlDocSetRootElement (doc, node_root);

  GList *it = NULL;

  for (it = priv->mp_tables; it; it=it->next)
    if (!gda_db_buildable_write_node (GDA_DB_BUILDABLE(GDA_DB_TABLE(it->data)),
                                       node_root, error))
      goto on_error;

  for (it = priv->mp_views; it; it=it->next)
    if (!gda_db_buildable_write_node (GDA_DB_BUILDABLE(GDA_DB_VIEW(it->data)),
                                       node_root,error))
      goto on_error;

  xmlSaveFormatFileEnc (path, doc, "UTF-8", 1);
  xmlFreeDoc (doc);
  xmlCleanupParser ();

  return TRUE;

on_error:
  xmlFreeDoc (doc);
  xmlCleanupParser ();
  return FALSE;
}

/**
 * gda_db_catalog_parse_file:
 * @self: an instance of #GdaDbCatalog
 * @xmlfile: xml file as #GFile instance
 * @error: Error container
 *
 * For detailed description see gda_db_catalog_parse_file_from_path()
 *
 * Since: 6.0
 */
gboolean
gda_db_catalog_parse_file (GdaDbCatalog *self,
                           GFile *xmlfile,
                           GError **error)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (xmlfile, FALSE);

  GFileInputStream *istream = NULL;
  xmlDocPtr doc = NULL;

  istream = g_file_read (xmlfile, NULL, error);

  if(!istream)
    {
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_FILE_READ,
                   _("Can't open stream for reading file '%s'"), g_file_get_basename (xmlfile));
      return FALSE;
    }

  xmlParserCtxtPtr ctxt = NULL;
  int buffer_size = 10;
  char buffer[buffer_size];
  int ctxt_res = 0;
  gchar *uri;
  ctxt = xmlCreatePushParserCtxt (NULL, NULL, buffer, ctxt_res, NULL);
  if (!ctxt)
    {
      uri = g_file_get_uri (xmlfile);
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_FILE_READ,
                   _("Can't create parse context for '%s'"), uri);
      g_free (uri);
      goto on_error;
    }

  int rescheck;
  while ((ctxt_res = g_input_stream_read ((GInputStream*)istream, buffer, buffer_size-1,
                                          NULL, error)) > 0)
    {
      rescheck = xmlParseChunk (ctxt, buffer, ctxt_res, 0);

      if (rescheck != 0)
        {
          g_set_error (error,
                       GDA_DB_CATALOG_ERROR,
                       GDA_DB_CATALOG_PARSE_CHUNK,
                       _("Error during parsing with error '%d'"), rescheck);

          goto on_error;
        }
    }

  xmlParseChunk (ctxt, buffer, 0, 1);

  doc = ctxt->myDoc;
  ctxt_res = ctxt->wellFormed;

  if (!ctxt_res)
    {
      uri = g_file_get_uri (xmlfile);
       g_set_error (error,
                    GDA_DB_CATALOG_ERROR,
                    GDA_DB_CATALOG_PARSE,
                    _("Failed to parse file: '%s'"), uri);
      g_free (uri);
      goto on_error;
    }

  if (!doc)
    {
      uri = g_file_get_uri (xmlfile);
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_DOC_NULL,
                   _("XML Document can't be created from file: '%s'"), uri);
      g_free (uri);
      goto on_error;
    }

  if (!_gda_db_catalog_validate_doc (doc, error))
    {
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_INVALID_XML,
                   _("XML file is not valid\n"));
      goto on_error;
    }

  if (!_gda_db_catalog_parse_doc (self, doc, error))
    goto on_error;

  g_input_stream_close (G_INPUT_STREAM(istream), NULL, error);
  xmlFreeParserCtxt (ctxt);
  xmlFreeDoc (doc);
  g_object_unref (istream);
  return TRUE;

on_error:
  g_input_stream_close (G_INPUT_STREAM(istream), NULL, error);
  g_object_unref (istream);
  if (ctxt)
    xmlFreeParserCtxt (ctxt);
  if (doc)
    xmlFreeDoc (doc);

  return FALSE;
}
