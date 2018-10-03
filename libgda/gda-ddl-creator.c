/* gda-ddl-creator.c
 *
 * Copyright (C) 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
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

#include "gda-ddl-creator.h"
#include "gda-ddl-table.h"
#include "gda-ddl-view.h"
#include "gda-ddl-fkey.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n-lib.h>

#include "gda-server-provider.h"
#include <libgda/gda-lockable.h>
#include "gda-meta-struct.h"

G_DEFINE_QUARK (gda_ddl_creator_error, gda_ddl_creator_error)

typedef struct
{
  GList *mp_tables; /* List of all tables that should be create, GdaDdlTable  */
  GList *mp_views; /* List of all views that should be created, GdaDdlView */
  gchar *mp_schemaname;
  GdaConnection *cnc;
} GdaDdlCreatorPrivate;

/**
 * SECTION:gda-ddl-creator
 * @title: GdaDdlCreator
 * @short_description: Object to constract database representation from an xml file or by reading the existing datatabase 
 * @see_also: #GdaDdlTable, #GdaDdlView
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This is a main object that represents overall database. In can be constracted from an xml 
 * file using gda_ddl_creator_parse_file() or gda_ddl_creator_parse_file_from_path(). It can also
 * be constracted from an open connection using gda_ddl_creator_parse_cnc(). The database can be
 * updated using gda_ddl_creator_perform_operation() and dumped to a file using
 * gda_ddl_creator_write_to_path() and gda_ddl_creator_write_to_file(). 
 *  
 */

G_DEFINE_TYPE_WITH_PRIVATE (GdaDdlCreator, gda_ddl_creator, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SCHEMA_NAME,
  PROP_CNC,
  /*<private>*/
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = {NULL};

/* Originally defined in gda-init.c */
extern xmlDtdPtr _gda_ddl_creator_dtd;

/**
 * gda_ddl_creator_new:
 *
 * Create new instance of #GdaDdlCreator.
 *
 * Returns: a new instance of #GdaDdlCreator
 *
 * Since: 6.0
 */
GdaDdlCreator*
gda_ddl_creator_new (void)
{
  return g_object_new (GDA_TYPE_DDL_CREATOR, NULL);
}

static void
gda_ddl_creator_finalize (GObject *object)
{
  GdaDdlCreator *self = GDA_DDL_CREATOR(object);
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  g_free (priv->mp_schemaname);

  G_OBJECT_CLASS (gda_ddl_creator_parent_class)->finalize (object);
}

static void
gda_ddl_creator_dispose (GObject *object)
{
  GdaDdlCreator *self = GDA_DDL_CREATOR(object);
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  if (priv->mp_tables)
    g_list_free_full (priv->mp_tables, (GDestroyNotify) g_object_unref);

  if (priv->mp_views)
    g_list_free_full (priv->mp_views, (GDestroyNotify) g_object_unref);

  if (priv->cnc)
    g_object_unref (priv->cnc);

  G_OBJECT_CLASS (gda_ddl_creator_parent_class)->finalize (object);
}
static void
gda_ddl_creator_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GdaDdlCreator *self = GDA_DDL_CREATOR (object);
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  switch (prop_id)
    {
      case PROP_SCHEMA_NAME:
        g_value_set_string (value, priv->mp_schemaname);
      break;
      case PROP_CNC:
        g_value_set_object (value, G_OBJECT (priv->cnc));  
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gda_ddl_creator_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GdaDdlCreator *self = GDA_DDL_CREATOR (object);
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

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
gda_ddl_creator_class_init (GdaDdlCreatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_ddl_creator_finalize;
  object_class->dispose = gda_ddl_creator_dispose;
  object_class->get_property = gda_ddl_creator_get_property;
  object_class->set_property = gda_ddl_creator_set_property;

  properties[PROP_SCHEMA_NAME] =
    g_param_spec_string ("schema_name",
                         "schema-name",
                         "Name of the schema",
                         NULL,
                         G_PARAM_READWRITE);

  properties[PROP_CNC] =
    g_param_spec_object ("connection",
                         "Connection",
                         "Connection to DB",
                         GDA_TYPE_CONNECTION,
                         G_PARAM_READWRITE);

  g_object_class_install_properties (object_class,N_PROPS,properties);
}

static void
gda_ddl_creator_init (GdaDdlCreator *self)
{
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  priv->mp_tables = NULL;
  priv->mp_views = NULL;
  priv->mp_schemaname = NULL;
  priv->cnc = NULL;
}

static gboolean
_gda_ddl_creator_validate_doc (xmlDocPtr doc,
                               GError **error)
{
  g_return_val_if_fail (doc,FALSE);

  xmlValidCtxtPtr  ctx = NULL;

  ctx = xmlNewValidCtxt ();

  if (!ctx)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_CONTEXT_NULL,
                   _("Context to validate xml file can't be created."));
      goto on_error;
    }

  int valid = xmlValidateDtd (ctx,doc,_gda_ddl_creator_dtd);

  if (!valid)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_INVALID_XML,
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
 * _gda_ddl_creator_parse_doc:
 * @self: a #GdaDdlCreator instance
 * @doc: a #xmlDocPtr instance to parse
 * @error: error container
 *
 * Internal method to populate #GdaDdlCreator from @doc instance.
 *
 * Returns: %TRUE if no errors, %FALSE otherwise. 
 *
 */
static gboolean
_gda_ddl_creator_parse_doc (GdaDdlCreator *self,
                            xmlDocPtr doc,
                            GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (doc,FALSE);

  xmlChar         *schema_name = NULL;
  xmlNodePtr       node        = NULL;

  node = xmlDocGetRootElement (doc);

  if (!node || g_strcmp0 ((gchar *)node->name, "schema") != 0)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_INVALID_SCHEMA,
                   _("Root node should be <schema>."));
      goto on_error;
    }

  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  schema_name = xmlGetProp(node,(xmlChar *)"name");
//  g_assert (schema_name); /* If schema_name == NULL we have problem with validation */
  if (schema_name)
    g_object_set (self,"schema_name",(char*)schema_name,NULL);

  xmlFree (schema_name);

  /* walk through the xmlDoc */
  for (node = node->children; node; node = node->next)
    {
    /* <table> tag for table creation */
    if (!g_strcmp0 ((gchar *) node->name, "table"))
      {
        GdaDdlTable *table;
        table = gda_ddl_table_new ();
        gda_ddl_base_set_schema (GDA_DDL_BASE(table), priv->mp_schemaname);

        if (!gda_ddl_buildable_parse_node (GDA_DDL_BUILDABLE(table), node, error))
          {
            g_object_unref (table);
            goto on_error;
          }
        else
            priv->mp_tables = g_list_append (priv->mp_tables,table);
      }
    else if (!g_strcmp0 ((gchar *) node->name, "view"))
      {
        GdaDdlView *view;
        view = gda_ddl_view_new ();
        gda_ddl_base_set_schema (GDA_DDL_BASE(view), priv->mp_schemaname);

        if (!gda_ddl_buildable_parse_node (GDA_DDL_BUILDABLE(view), node, error))
          {
            g_object_unref (view);
            goto on_error;
          }
        else
          priv->mp_views = g_list_append (priv->mp_views,view);
      }  /* end of else if */
  } /* End of for loop */

  xmlFree (node);
  return TRUE;

on_error:
  if (node) xmlFree (node);
  return FALSE;
}

/**
 * gda_ddl_creator_parse_file_from_path:
 * @self: an instance of #GdaDdlCreator
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
 * <!ELEMENT table (comment?,column+, fkey*)>
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
 * [libgda-ddl-creator.dtd]
 * (https://gitlab.gnome.org/GNOME/libgda/blob/master/libgda/libgda-ddl-creator.dtd)
 *
 * The given @xmlfile will be checked before parsing and %FALSE will be
 * returned if fails. The @xmlfile will be validated internally.
 * The same method can be used to validate xmlfile before parsing it.
 */
gboolean
gda_ddl_creator_parse_file_from_path (GdaDdlCreator *self,
                                      const gchar *xmlfile,
                                      GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (xmlfile,FALSE);
  g_return_val_if_fail (_gda_ddl_creator_dtd,FALSE);

  xmlDocPtr doc = NULL;

  doc = xmlParseFile(xmlfile);

  if (!doc)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_DOC_NULL,
                   _("xmlDoc object can't be created from xmfile name '%s'"), xmlfile);
      goto on_error;
    }

  if (!_gda_ddl_creator_validate_doc (doc,error))
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_INVALID_XML,
                   _("xml file '%s' is not valid\n"), xmlfile);
      goto on_error;
    }

  if (!_gda_ddl_creator_parse_doc (self,doc,error))
    goto on_error;
  
  xmlFreeDoc (doc);
  return TRUE;

on_error:
  if (doc) xmlFreeDoc (doc);

  return FALSE;
}

/**
 * gda_ddl_creator_validate_file_from_path:
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
gda_ddl_creator_validate_file_from_path (const gchar *xmlfile,
                                         GError **error)
{
  g_return_val_if_fail (xmlfile,FALSE);
  g_return_val_if_fail (_gda_ddl_creator_dtd,FALSE);

  xmlDocPtr doc = NULL;

  doc = xmlParseFile(xmlfile);

  if (!doc)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_DOC_NULL,
                   _("xmlDoc object can't be created from xmfile name '%s'"), xmlfile);
      goto on_error;
    }

  if (!_gda_ddl_creator_validate_doc (doc,error))
    goto on_error;

  xmlFreeDoc (doc);

  return TRUE;

on_error:
  if (doc)  xmlFreeDoc (doc);
  return FALSE;
}

/**
 * gda_ddl_creator_get_tables:
 * @self: a #GdaDdlCreator object
 *
 * Returns: (element-type Gda.DdlTable) (transfer none): a list of tables
 *
 * Since: 6.0
 */
GList*
gda_ddl_creator_get_tables (GdaDdlCreator *self)
{
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);
  return priv->mp_tables;
}

/**
 * gda_ddl_creator_get_views:
 * @self: a #GdaDdlCreator object
 *
 * Returns: (element-type Gda.DdlView) (transfer none): a list of views
 *
 * Since: 6.0
 */
GList*
gda_ddl_creator_get_views (GdaDdlCreator *self)
{
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);
  return priv->mp_views;
}

/**
 * gda_ddl_creator_get_table:
 * @self: a #GdaDdlCreator object
 * @name: table name
 *
 * Convenient function to return a table based on name
 *
 * Returns: table as #GdaDdlTable or %NULL
 *
 */
const GdaDdlTable*
gda_ddl_creator_get_table (GdaDdlCreator *self,
                           const gchar *catalog,
                           const gchar *schema,
                           const gchar *name)
{
  g_return_val_if_fail (self,NULL);
  g_return_val_if_fail (name,NULL);

  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  GdaDdlBase *iobj = gda_ddl_base_new ();
  gda_ddl_base_set_names (iobj,catalog,schema,name);

  GList *it = NULL;

  for (it = priv->mp_tables; it; it = it->next)
      if (!gda_ddl_base_compare (iobj,GDA_DDL_BASE(it->data)))
        {
          g_object_unref (iobj);
          return GDA_DDL_TABLE(it->data);
        }

  g_object_unref (iobj);
  return NULL;
}

/**
 * gda_ddl_creator_get_view:
 * @self: a #GdaDdlCreator object
 * @name: view name
 *
 * Convenient function to return a view based on name
 *
 * Returns: table as #GdaDdlView or %NULL
 *
 */
const GdaDdlView*
gda_ddl_creator_get_view (GdaDdlCreator *self,
                          const gchar *catalog,
                          const gchar *schema,
                          const gchar *name)
{
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  GList *it = NULL;

  GdaDdlBase *iobj = gda_ddl_base_new ();
  gda_ddl_base_set_names (iobj,catalog,schema,name);

  for (it = priv->mp_views; it; it = it->next)
    if (!gda_ddl_base_compare (iobj,GDA_DDL_BASE(it)))
      {
        g_object_unref (iobj);
      return GDA_DDL_VIEW(it);
      }

  g_object_unref (iobj);
  return NULL;
}

/** 
 * gda_ddl_creator_parse_cnc:
 * @self: a #GdaDdlCreator instance 
 * @error: error storage object
 *
 * Parse cnc to populate @self object. This method should be called every time after database was
 * modified or @self was just created.  
 *
 * Returns: Returns %TRUE if succeeded, %FALSE otherwise.
 */
gboolean
gda_ddl_creator_parse_cnc (GdaDdlCreator *self,
                           GError **error)
{
  g_return_val_if_fail (self,FALSE);

  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  if (!gda_connection_is_opened (priv->cnc))
    return FALSE;

  if(!gda_connection_update_meta_store (priv->cnc, NULL, error))
    return FALSE;


  GdaMetaStore *mstore = NULL;
  GdaMetaStruct *mstruct = NULL;

  mstore = gda_connection_get_meta_store (priv->cnc);
  mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", mstore, "features", GDA_META_STRUCT_FEATURE_ALL, NULL);

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
          GdaDdlTable *table = gda_ddl_table_new_from_meta(it->data);
          if (!table)
            continue;

          priv->mp_tables = g_list_append (priv->mp_tables,table);
          continue;
        }

      if(GDA_META_DB_OBJECT(it->data)->obj_type == GDA_META_DB_VIEW)
        {
          GdaDdlView *view = gda_ddl_view_new_from_meta(it->data);
          priv->mp_views = g_list_append (priv->mp_views,view);
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
 * gda_ddl_creator_append_table:
 * @self: a #GdaDdlCreator instance
 * @table: table to append
 *
 * This method append @table to the total list of all tables stored in @self.
 *
 * Since: 6.0
 */
void
gda_ddl_creator_append_table (GdaDdlCreator *self,
                              GdaDdlTable *table)
{
  g_return_if_fail (self);

  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  priv->mp_tables = g_list_append (priv->mp_tables,g_object_ref (table));
}

/**
 * gda_ddl_creator_append_view:
 * @self: a #GdaDdlCreator instance
 * @view: view to append
 *
 * This method append @view to the total list of all views stored in @self.
 *
 * Since: 6.0
 */
void
gda_ddl_creator_append_view (GdaDdlCreator *self,
                             GdaDdlView *view)
{
  g_return_if_fail (self);

  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  priv->mp_views = g_list_append (priv->mp_views,g_object_ref (view));
}

/**
 * gda_ddl_creator_perform_operation:
 * @self: a #GdaDdlCreator object
 * @error: object to store error
 *
 * After population @self with all data this method may be
 * called to trigger code and modify user database. This is the main
 * method to work with database. For retrieving information from database to an
 * xml file use gda_ddl_creator_parse_cnc() and gda_ddl_buildable_write_xml().
 *
 * Connection should be opened to use this method. See gda_connection_open()
 * method for reference.
 *
 * Only table can be created. Views are ignored
 *
 * Each table from database compared with each table in the #GdaDdlCreator
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
gda_ddl_creator_perform_operation (GdaDdlCreator *self,
                                   GError **error)
{
  g_return_val_if_fail (self,FALSE);

  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  if (!gda_connection_is_opened (priv->cnc))
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_CONNECTION_CLOSED,
                   _("Connection is not open"));
      return FALSE;
    }

  gda_lockable_lock ((GdaLockable*)priv->cnc);
// We need to get MetaData
  if(!gda_connection_update_meta_store (priv->cnc, NULL, error))
    return FALSE;

  GdaMetaStore *mstore = gda_connection_get_meta_store (priv->cnc);
  GdaMetaStruct *mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", mstore, "features", GDA_META_STRUCT_FEATURE_ALL, NULL);

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
      g_value_set_string (catalog,gda_ddl_base_get_catalog(it->data));
      g_value_set_string (schema ,gda_ddl_base_get_schema (it->data));
      g_value_set_string (name   ,gda_ddl_base_get_name   (it->data));

      mobj = gda_meta_struct_complement (mstruct,GDA_META_DB_TABLE,catalog,schema,name,NULL);

      if (mobj)
        {
          if(!gda_ddl_table_update (it->data,GDA_META_TABLE(mobj),priv->cnc,error))
            goto on_error;
        }
      else
        {
          if(!gda_ddl_table_create (it->data,priv->cnc,TRUE,error))
            goto on_error;
        }
    } /* End of for loop */

  gda_value_free (catalog);
  gda_value_free (schema);
  gda_value_free (name);

/*TODO: add update option for views */
  for (it = priv->mp_views; it; it = it->next)
    {
      if(!gda_ddl_view_create (it->data,priv->cnc,error))
        goto on_error;
    } /* End of for loop */

  g_object_unref (mstruct);
  gda_lockable_unlock ((GdaLockable*)priv->cnc);

  return TRUE;

on_error:
  gda_value_free (catalog);
  gda_value_free (schema);
  gda_value_free (name);
  g_object_unref (mstruct);
  gda_lockable_unlock ((GdaLockable*)priv->cnc);

  return FALSE;
}

/**
 * gda_ddl_creator_write_to_file:
 * @self: a #GdaDdlCreator instance
 * @file: a #GFile to write database description
 * @error: container to hold error 
 * 
 * This method writes database description as xml file.
 * Similar to gda_ddl_creator_write_to_path()
 *
 * Returns: %TRUE if no error occurred, %FALSE otherwise.
 *
 * Since: 6.0
 */
gboolean
gda_ddl_creator_write_to_file (GdaDdlCreator *self,
                               GFile *file,
                               GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (file,FALSE);
  
  gchar *filepath = g_file_get_path (file);
  gboolean res = gda_ddl_creator_write_to_path (self,filepath,error);
  g_free (filepath);
  return res;
}

/**
 * gda_ddl_creator_write_to_path:
 * @self: a #GdaDdlCreator instance
 * @path: path to xml file to save #GdaDdlCreator
 * @error: container to hold an error
 *
 * Save content of @self to a user friendly xml file.
 *
 * Returns: %TRUE is no error, %FLASE otherwise. 
 *
 * Since: 6.0
 */
gboolean
gda_ddl_creator_write_to_path (GdaDdlCreator *self,
                               const gchar *path,
                               GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (path,FALSE);
 
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  xmlDocPtr doc = NULL;
  xmlNodePtr node_root = NULL;

  doc = xmlNewDoc (BAD_CAST "1.0");
  node_root = xmlNewNode (NULL, BAD_CAST "schema");
  
  if (priv->mp_schemaname)
    xmlNewProp (node_root,(xmlChar*)"name",(xmlChar*)priv->mp_schemaname);

  xmlDocSetRootElement (doc,node_root);

  GList *it = NULL;
  
  for (it = priv->mp_tables; it; it=it->next)
    if (!gda_ddl_buildable_write_node (GDA_DDL_BUILDABLE(GDA_DDL_TABLE(it->data)),
                                       node_root,error))
      goto on_error;

  for (it = priv->mp_views; it; it=it->next)
    if (!gda_ddl_buildable_write_node (GDA_DDL_BUILDABLE(GDA_DDL_VIEW(it->data)),
                                       node_root,error))
      goto on_error;

  xmlSaveFormatFileEnc (path,doc,"UTF-8",1);
  xmlFreeDoc (doc);
  xmlCleanupParser();
  
  return TRUE;

on_error:
  xmlFreeDoc (doc);
  xmlCleanupParser();
  return FALSE;
}

/**
 * gda_ddl_creator_parse_file:
 * @self: an instance of #GdaDdlCreator
 * @xmlfile: xml file as #GFile instance
 * @error: Error container
 * 
 * For detailed description see gda_ddl_creator_parse_file_from_path()
 *
 * Since: 6.0
 */
gboolean
gda_ddl_creator_parse_file (GdaDdlCreator *self,
                            GFile *xmlfile,
                            GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (xmlfile,FALSE);
  g_return_val_if_fail (_gda_ddl_creator_dtd,FALSE);

  GFileInputStream *istream = NULL;
  xmlDocPtr doc = NULL;

  istream = g_file_read (xmlfile,NULL,error);

  if(!istream)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_FILE_READ,
                   _("Can't open stream for reading file '%s'"), g_file_get_basename(xmlfile));
      return FALSE;
    }

  xmlParserCtxtPtr ctxt = NULL;
  char buffer[10];
  int ctxt_res = 0;
  ctxt = xmlCreatePushParserCtxt (NULL,NULL,buffer,ctxt_res,NULL);
  if (!ctxt)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_FILE_READ,
                   _("Can't create parse context for '%s'"), g_file_get_basename(xmlfile));
      goto on_error;
    }

  int rescheck;
  while ((ctxt_res = g_input_stream_read ((GInputStream*)istream,buffer,9,NULL,error)) > 0)
    {
      rescheck = xmlParseChunk(ctxt,buffer,ctxt_res,0); 
   
      if (rescheck != 0)
        {
          g_set_error (error,
                       GDA_DDL_CREATOR_ERROR,
                       GDA_DDL_CREATOR_PARSE_CHUNK,
                       _("Error during xmlParseChunk with error '%d'"), rescheck);

          goto on_error;
        }
    }

  xmlParseChunk (ctxt,buffer,0,1);

  doc = ctxt->myDoc;
  ctxt_res = ctxt->wellFormed;

  if (!ctxt_res)
    {
       g_set_error (error,
                    GDA_DDL_CREATOR_ERROR,
                    GDA_DDL_CREATOR_PARSE,
                    _("Failed to parse file '%s'"), g_file_get_basename (xmlfile));
      goto on_error;
    }

  if (!doc)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_DOC_NULL,
                   _("xmlDoc object can't be created from xmfile name"));
      goto on_error;
    }

  if (!_gda_ddl_creator_validate_doc(doc, error))
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_INVALID_XML,
                   _("xml file is not valid\n"));
      goto on_error;
    }

  if (!_gda_ddl_creator_parse_doc (self, doc, error))
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

gboolean
gda_ddl_creator_drop_table (GdaDdlCreator *self,
                            const gchar* table,
                            GError **error)
{
  g_return_val_if_fail (GDA_IS_DDL_CREATOR(self),FALSE);
  g_return_val_if_fail (table,FALSE);

  GdaConnection *cnc;
  g_object_get (self,"connection",&cnc,NULL);

  if (!gda_connection_is_opened (cnc))
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_CONNECTION_CLOSED,
                   _("Connection is not open"));
      return FALSE;
    }

  gda_lockable_lock (GDA_LOCKABLE(cnc));

  GdaServerOperation *op = gda_connection_create_operation (cnc,GDA_SERVER_OPERATION_DROP_TABLE,
                                                            NULL,error);
  if (!op)
    return FALSE;

  if (!gda_server_operation_set_value_at (op,table,error,"/TABLE_DESC_P/TABLE_NAME"))
    goto onerror;

  if (!gda_connection_perform_operation (cnc,op,error))
    goto onerror;

  gda_lockable_unlock (GDA_LOCKABLE(cnc));

  return TRUE;

onerror:
  g_object_unref (op);
  return FALSE;
}


