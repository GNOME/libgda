/* gda-ddl-creator.c
 *
 * Copyright © 2018
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

//  gchar *dtd_file_path; /* path to DTD file for xml schema validation */
  gchar *mp_schemaname;
}GdaDdlCreatorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaDdlCreator, gda_ddl_creator, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SCHEMA_NAME,
  /*<private>*/
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = {NULL};

/* Orignally defined in gda-init.c */
extern xmlDtdPtr _gda_ddl_creator_dtd;

/**
 * gda_ddl_creator_new:
 *
 * Create new instance of #GdaDdlCreator
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

  if (priv->mp_tables)
    g_list_free_full (priv->mp_tables, (GDestroyNotify)gda_ddl_table_free);
  if (priv->mp_views)
    g_list_free_full (priv->mp_views, (GDestroyNotify)gda_ddl_view_free);

  g_free (priv->mp_schemaname);

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
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gda_ddl_creator_class_init (GdaDdlCreatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_ddl_creator_finalize;
  object_class->get_property = gda_ddl_creator_get_property;
  object_class->set_property = gda_ddl_creator_set_property;

  properties[PROP_SCHEMA_NAME] =
    g_param_spec_string ("schema_name",
                         "schema-name",
                         "Name of the schema",
                         NULL,
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
}

static gboolean
_gda_ddl_creator_validate_file_from_path (const gchar *xmlfile,
                                          xmlDocPtr doc,
                                          GError **error)
{
  g_return_val_if_fail (xmlfile,FALSE);
  g_return_val_if_fail (doc,FALSE);

  xmlValidCtxtPtr  ctx = NULL;

  ctx = xmlNewValidCtxt ();

  if (!ctx)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_CONTEXT_NULL,
                   _("Context to validte xml file can't be created."));
      goto on_error;
    }

  int valid = xmlValidateDtd (ctx,doc,_gda_ddl_creator_dtd);

  if (!valid)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_UNVALID_XML,
                   _("xml file '%s' is unvalid"), xmlfile);
      goto on_error;
    }

  xmlFreeValidCtxt (ctx);

  return TRUE;

on_error:
  if (ctx)  xmlFreeValidCtxt (ctx);
  return FALSE;
}

/**
 * gda_ddl_creator_parse_file_from_path:
 * @self: an instance of #GdaDdlCreator
 * @xmlfile: xml file to parse
 * @error: Error container
 *
 * This method reads information from @xmlfile and populate @self object.
 * The @xmlfile should correspon to the following DTD format:
 *
 * |[<!-- language="DTD" -->
 * <!ELEMENT schema (table+, view*)>
 * <!ATTLIST schema name ID #REQUIRED>
 *
 * <!ELEMENT table (comment?,column+, fkey*)>
 * <!ATTLIST table temptable	(TRUE|FALSE)	"FALSE">
 * <!ATTLIST table name		ID		#REQUIRED>
 * <!ATTLIST table space		CDATA		#IMPLIED>
 *
 * <!ELEMENT column (comment?, size?, default?, check?)>
 * <!ATTLIST column name		ID		#REQUIRED>
 * <!ATTLIST column type		CDATA		#REQUIRED>
 * <!ATTLIST column pkey		(TRUE|FALSE)	"FALSE">
 * <!ATTLIST column unique		(TRUE|FALSE)	"FALSE">
 * <!ATTLIST column autoinc	(TRUE|FALSE)	"FALSE">
 * <!ATTLIST column nnul		(TRUE|FALSE)	"FALSE">
 *
 * <!ELEMENT comment	(#PCDATA)>
 * <!ELEMENT size		(#PCDATA)>
 * <!ELEMENT default	(#PCDATA)>
 * <!ELEMENT check		(#PCDATA)>
 *
 * <!ELEMENT fkey (fk_field?)>
 * <!ATTLIST fkey reftable IDREF 							#IMPLIED>
 * <!ATTLIST fkey onupdate (RESTRICT|CASCADE|SET_NULL|NO_ACTION|SET_DEFAULT)	#IMPLIED>
 * <!ATTLIST fkey ondelete (RESTRICT|CASCADE|SET_NULL|NO_ACTION|SET_DEFAULT)	#IMPLIED>
 *
 * <!ELEMENT fk_field (#PCDATA)>
 * <!ATTLIST fk_field name	IDREF #REQUIRED>
 * <!ATTLIST fk_field reffield	IDREF #REQUIRED>
 *
 * <!ELEMENT view (definition)>
 * <!ATTLIST view name		ID		#REQUIRED>
 * <!ATTLIST view replace		(TRUE|FALSE)	"FALSE">
 * <!ATTLIST view temp		(TRUE|FALSE)	"FALSE">
 * <!ATTLIST view ifnotexists	(TRUE|FALSE)	"TRUE">
 *
 * <!ELEMENT definition (#PCDATA)>
 * ]|
 *
 * Up to day description of the xml file schema can be found in DTD file
 * [libgda-ddl-creator.dtd](https://gitlab.gnome.org/GNOME/libgda/blob/master/libgda/libgda-ddl-creator.dtd)
 *
 * The given @xmlfile will be checked before parsing and %FALSE will be
 * returned if fails. The @xmlfile will be validated internally using
 * gda_ddl_creator_validate_file_from_path(). The same method can be used to
 * validate xmlfile before parsing it.
 */
gboolean
gda_ddl_creator_parse_file_from_path (GdaDdlCreator *self,
                                      const gchar *xmlfile,
                                      GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (xmlfile,FALSE);
  g_return_val_if_fail (_gda_ddl_creator_dtd,FALSE);

  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  xmlDocPtr        doc         = NULL;
  xmlChar         *schema_name = NULL;
  xmlNodePtr       node        = NULL;

  doc = xmlParseFile(xmlfile);

  if (!doc)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_DOC_NULL,
                   _("xmlDoc object can't be created from xmfile name '%s'"), xmlfile);
      g_print ("Step %d\n",__LINE__);
      goto on_error;
    }

  if (!_gda_ddl_creator_validate_file_from_path(xmlfile,doc,error))
    return FALSE;
  node = xmlDocGetRootElement (doc);

  if (!node || g_strcmp0 ((gchar *)node->name, "schema") != 0)
    {
      g_set_error (error,
                   GDA_DDL_CREATOR_ERROR,
                   GDA_DDL_CREATOR_UNVALID_SCHEMA,
                   _("Root node of file '%s' should be <schema>."), xmlfile);
      goto on_error;
    }

  schema_name = xmlGetProp(node,(xmlChar *)"name");
  g_assert (schema_name); /* If schema_name == NULL we have problem with validation */
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

        if (!gda_ddl_buildable_parse_node (GDA_DDL_BUILDABLE(table), node, error))
          {
            gda_ddl_table_free (table);
            goto on_error;
          }
        else
            priv->mp_tables = g_list_append (priv->mp_tables,table);

      }
    else if (!g_strcmp0 ((gchar *) node->name, "view"))
      {
        GdaDdlView *view;
        view = gda_ddl_view_new ();

        if (!gda_ddl_buildable_parse_node (GDA_DDL_BUILDABLE(view), node, error))
          {
            gda_ddl_view_free (view);
            goto on_error;
          }
        else
          priv->mp_views = g_list_append (priv->mp_views,view);

      }  /* end of else if */
  } /* End of for loop */

  return TRUE;

on_error:
  if (doc) xmlFreeDoc (doc);
  if (node) xmlFree (node);

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
      g_print ("Step %d\n",__LINE__);
      goto on_error;
    }

  if (!_gda_ddl_creator_validate_file_from_path(xmlfile,doc,error))
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
 * Returns: a list of tables
 *
 */
const GList*
gda_ddl_creator_get_tables (GdaDdlCreator *self)
{
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);
  return priv->mp_tables;
}

/**
 * gda_ddl_creator_get_views:
 * @self: a #GdaDdlCreator object
 *
 * Returns: a list of views
 *
 */
const GList*
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
                           const gchar* name)
{
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  GList *it = NULL;

  for (it = priv->mp_tables; it; it = it->next)
      if (!g_strcmp0 (name, gda_ddl_base_get_name (GDA_DDL_BASE(it->data))))
        return GDA_DDL_TABLE(it->data);
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
                          const gchar* name)
{
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  GList *it = NULL;

  for (it = priv->mp_views; it; it = it->next)
    if (!g_strcmp0 (name, gda_ddl_base_get_name (GDA_DDL_BASE(it))))
      return GDA_DDL_VIEW(it);
  return NULL;
}

/** 
 * gda_ddl_creator_parse_cnc:
 * @self:
 * @cnc:
 * @error:
 *
 *
 *
 */
gboolean
gda_ddl_creator_parse_cnc (GdaDdlCreator *self,
                           GdaConnection *cnc,
                           GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (cnc,FALSE);

  if (!gda_connection_is_opened (cnc))
    return FALSE;

  if(!gda_connection_update_meta_store (cnc, NULL, error))
    return FALSE;

  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  GdaMetaStore *mstore = NULL;
  GdaMetaStruct *mstruct = NULL;

  mstore = gda_connection_get_meta_store (cnc);
  mstruct = gda_meta_struct_new (mstore,GDA_META_STRUCT_FEATURE_ALL);

  if(!gda_meta_struct_complement_all (mstruct,error))
    goto on_error;

  GSList *dblist = NULL;

  dblist = gda_meta_struct_get_all_db_objects (mstruct);

  if (!dblist)
    goto on_error;

  GSList *it = NULL;

  for (it=dblist; it; it = it->next)
    {
      if(GDA_META_DB_OBJECT(it->data)->obj_type == GDA_META_DB_TABLE)
        {
          GdaDdlTable *table = gda_ddl_table_new_from_meta(it->data);
          priv->mp_tables = g_list_append (priv->mp_tables,table);
        }
      g_print ("Object is: %s\n",GDA_META_DB_OBJECT (it->data)->obj_full_name);
    }

  g_slist_free (dblist);
  g_object_unref (mstruct);
  return TRUE;

on_error:
  g_slist_free (dblist);
  g_object_unref (mstruct);
  return FALSE;
}

/**
 * gda_ddl_creator_free:
 * @self: a #GdaDdlCreator object
 *
 * Convenient method to fdelete the object and free the memory.
 *
 * Since: 6.0
 */
void
gda_ddl_creator_free (GdaDdlCreator *self)
{
  g_clear_object (&self);
}

void
gda_ddl_creator_append_table (GdaDdlCreator *self,
                              const GdaDdlTable *table)
{
  g_return_if_fail (self);

  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  priv->mp_tables = g_list_append (priv->mp_tables,(gpointer)table);
}

void
gda_ddl_creator_append_view (GdaDdlCreator *self,
                             const GdaDdlTable *view)
{
  g_return_if_fail (self);

  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private (self);

  priv->mp_views = g_list_append (priv->mp_views,(gpointer)view);


}

/**
 * gda_ddl_creator_perform_operation:
 * @self: a #GdaDdlCreator object
 * @cnc: a connection object to work with
 * @mode: mode of operation
 * @error: object to store error
 *
 * After population #GdaDdlCreator instance with all data this method may be
 * called to trigger the code and modify user database. This is the main
 * method to work with database. For retreving information from database to an
 * xml file use gda_ddl_creator_parse_cnc() and gda_ddl_buildable_write_xml().
 *
 * Connection should be opened to use this method. See gda_connectio_open()
 * methods for reference.
 *
 * Only table can be created. Views are ignored
 *
 * UPDATE:
 *    Each table from database compared with each table in the #GdaDdlCreator
 *    instance. If table doesn't exist, it will be created (CREATE_TABLE). If
 *    table exists in db and xml file, all column will be checked. If column
 *    is present in xml file but not in db it will be created (ADD_COLUMN). If
 *    column exists but has different parameters, e.g. nonnull it will not be
 *    modified. Pkey will be compared too and added to DB if missed.
 *    The existing data will not be modified.
 *
 * REPLACE:
 *    New object will be added, e.g. tables, columns, fkeys. If the object
 *    exist all parameters will be checked and changed appropriatly.
 *
 */
gboolean
gda_ddl_creator_perform_operation (GdaDdlCreator *self,
                                   GdaConnection *cnc,
                                   GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (cnc,FALSE);

  if (!gda_connection_is_opened(cnc))
    {
      g_print ("Connection is not open\n");
      return FALSE;
    }

  g_print ("%s:%d\n",__FILE__,__LINE__);
  gda_lockable_lock((GdaLockable*)cnc);
// We need to get MetaData
  if(!gda_connection_update_meta_store(cnc,NULL,error))
    goto on_error;

  g_print ("%s:%d\n",__FILE__,__LINE__);
  GdaMetaStore *mstore = gda_connection_get_meta_store(cnc);
  GdaMetaStruct *mstruct = gda_meta_struct_new(mstore,GDA_META_STRUCT_FEATURE_ALL);

  g_print ("%s:%d\n",__FILE__,__LINE__);
  // We need information about catalog, schema, name for each object we would
  // like to check
  GdaDdlCreatorPrivate *priv = gda_ddl_creator_get_instance_private(self);

  g_print ("%s:%d\n",__FILE__,__LINE__);

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
      g_value_set_string (catalog,gda_ddl_base_get_catalog(it->data));
      g_value_set_string (schema ,gda_ddl_base_get_schema (it->data));
      g_value_set_string (name   ,gda_ddl_base_get_name   (it->data));

      mobj = gda_meta_struct_complement(mstruct,GDA_META_DB_TABLE,
                                        catalog,
                                        schema,
                                        name,
                                        error);

      if (mobj)
        {
          g_print ("%s:%d\n",__FILE__,__LINE__);
          g_print ("At %d object name = %s\n",__LINE__,mobj->obj_full_name);
          if(!gda_ddl_table_update (it->data,GDA_META_TABLE(mobj),cnc,error))
            {
              g_print ("%s:%d\n",__FILE__,__LINE__);
              goto on_error;
            }
        }
      else
        {
          g_print ("%s:%d\n",__FILE__,__LINE__);
          if(!gda_ddl_table_create (it->data,cnc,error))
            {
              g_print ("%s:%d\n",__FILE__,__LINE__);
              goto on_error;
            }
          g_print ("%s:%d\n",__FILE__,__LINE__);
        }
    } /* End of foor loop */

  gda_value_free (catalog);
  gda_value_free (schema);
  gda_value_free (name);

  for (it = priv->mp_views; it; it = it->next)
    {
      if(!gda_ddl_view_create (it->data,cnc,error))
        {
           g_print ("%s:%d\n",__FILE__,__LINE__);
           goto on_error;
        }
    } /* End of foor loop */

  g_print ("%s:%d\n",__FILE__,__LINE__);
  g_object_unref (mstruct);
  gda_lockable_unlock((GdaLockable*)cnc);

  return TRUE;

on_error:
  g_print ("%s:%d\n",__FILE__,__LINE__);
  gda_value_free (catalog);
  gda_value_free (schema);
  gda_value_free (name);
  g_object_unref (mstruct);
  gda_lockable_unlock((GdaLockable*)cnc);

  return FALSE;
}

