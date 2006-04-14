/* gda-dict.h
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


/*
 * This object manages the objects used my mergeant (for tables, queries, etc),
 * but not what is specific to Mergeant's own GUI.
 */

#ifndef __GDA_DICT_H_
#define __GDA_DICT_H_

#include <glib-object.h>
#include "gda-decl.h"
#include "gda-value.h"
#include "gda-enums.h"

G_BEGIN_DECLS

#define GDA_TYPE_DICT          (gda_dict_get_type())
#define GDA_DICT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_dict_get_type(), GdaDict)
#define GDA_DICT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_dict_get_type (), GdaDictClass)
#define GDA_IS_DICT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_dict_get_type ())


/* error reporting */
extern GQuark gda_dict_error_quark (void);
#define GDA_DICT_ERROR gda_dict_error_quark ()

enum {
	GDA_DICT_META_DATA_UPDATE_ERROR,
	GDA_DICT_META_DATA_UPDATE_USER_STOPPED,
	GDA_DICT_LOAD_FILE_NOT_EXIST_ERROR,
	GDA_DICT_FILE_LOAD_ERROR,
	GDA_DICT_FILE_SAVE_ERROR,
	GDA_DICT_PROPOSED_FILE,
	GDA_DICT_DATATYPE_ERROR,
	GDA_DICT_FUNCTIONS_ERROR,
	GDA_DICT_AGGREGATES_ERROR,
};


/* struct for the object's data */
struct _GdaDict
{
	GObject          object;
	GdaDictPrivate  *priv;
};

/* struct for the object's class */
struct _GdaDictClass
{
	GObjectClass     parent_class;

        /* signal the addition or removal of a query in the queries list */
        void (*query_added)      (GdaDict * dict, GdaQuery *new_query);
        void (*query_removed)    (GdaDict * dict, GdaQuery *old_query);
        void (*query_updated)    (GdaDict * dict, GdaQuery *query);

        /* signal the addition or removal of a graph in the graphs list */
        void (*graph_added)      (GdaDict * dict, GdaGraph *new_graph);
        void (*graph_removed)    (GdaDict * dict, GdaGraph *old_graph);
        void (*graph_updated)    (GdaDict * dict, GdaGraph *graph);

	void (*data_type_added)      (GdaDict *obj, GdaDictType *type);
        void (*data_type_removed)    (GdaDict *obj, GdaDictType *type);
        void (*data_type_updated)    (GdaDict *obj, GdaDictType *type);

        void (*function_added)       (GdaDict *obj, GdaDictFunction *function);
        void (*function_removed)     (GdaDict *obj, GdaDictFunction *function);
        void (*function_updated)     (GdaDict *obj, GdaDictFunction *function);

        void (*aggregate_added)      (GdaDict *obj, GdaDictAggregate *aggregate);
        void (*aggregate_removed)    (GdaDict *obj, GdaDictAggregate *aggregate);
        void (*aggregate_updated)    (GdaDict *obj, GdaDictAggregate *aggregate);

        void (*data_update_started)       (GdaDict *obj);
        void (*update_progress)           (GdaDict *obj, gchar * msg, guint now, guint total);
        void (*data_update_finished)      (GdaDict *obj);

	/* signal that a change in the whole dictionary has occured */
	void (*changed)          (GdaDict * dict);
};

GType           gda_dict_get_type            (void);
GObject        *gda_dict_new                 (void);

void            gda_dict_set_connection      (GdaDict *dict, GdaConnection *cnc);
GdaConnection  *gda_dict_get_connection      (GdaDict *dict);

guint           gda_dict_get_id_serial       (GdaDict *dict);
void            gda_dict_set_id_serial       (GdaDict *dict, guint value);

void            gda_dict_declare_object_string_id_change (GdaDict *dict, GdaObject *obj, const gchar *oldid);
GdaObject      *gda_dict_get_object_by_string_id (GdaDict *dict, const gchar *strid);

gboolean        gda_dict_update_dbms_data    (GdaDict *dict, GError **error);
void            gda_dict_stop_update_dbms_data (GdaDict *dict);

gchar          *gda_dict_compute_xml_filename(GdaDict *dict, const gchar *datasource, 
					      const gchar *app_id, GError **error);
void            gda_dict_set_xml_filename    (GdaDict *dict, const gchar *xmlfile);
const gchar    *gda_dict_get_xml_filename    (GdaDict *dict);
gboolean        gda_dict_load                (GdaDict *dict, GError **error);
gboolean        gda_dict_save                (GdaDict *dict, GError **error);
gboolean        gda_dict_load_xml_file       (GdaDict *dict, const gchar *xmlfile, GError **error);
gboolean        gda_dict_save_xml_file       (GdaDict *dict, const gchar *xmlfile, GError **error);

GdaDataHandler *gda_dict_get_handler         (GdaDict *dict, GdaValueType for_type);
GdaDataHandler *gda_dict_get_default_handler (GdaDict *dict, GdaValueType for_type);

/* GdaDictType manipulations */
GSList         *gda_dict_get_data_types            (GdaDict *dict);
GdaDictType    *gda_dict_get_data_type_by_name     (GdaDict *dict, const gchar *type_name);
GdaDictType    *gda_dict_get_data_type_by_xml_id   (GdaDict *dict, const gchar *xml_id);
gboolean        gda_dict_declare_custom_data_type  (GdaDict *dict, GdaDictType *type);

/* GdaDictFunction manipulations */
GSList           *gda_dict_get_functions             (GdaDict *dict);
GSList           *gda_dict_get_functions_by_name     (GdaDict *dict, const gchar *funcname);
GdaDictFunction  *gda_dict_get_function_by_name_arg  (GdaDict *dict, const gchar *funcname,
						      const GSList *argtypes);
GdaDictFunction  *gda_dict_get_function_by_xml_id    (GdaDict *dict, const gchar *xml_id);
GdaDictFunction  *gda_dict_get_function_by_dbms_id   (GdaDict *dict, const gchar *dbms_id);

/* GdaDictAggregate manipulations */
GSList           *gda_dict_get_aggregates            (GdaDict *dict);
GSList           *gda_dict_get_aggregates_by_name    (GdaDict *dict, const gchar *aggname);
GdaDictAggregate *gda_dict_get_aggregate_by_name_arg (GdaDict *dict, const gchar *aggname,
								GdaDictType *argtype);
GdaDictAggregate *gda_dict_get_aggregate_by_xml_id   (GdaDict *dict, const gchar *xml_id);
GdaDictAggregate *gda_dict_get_aggregate_by_dbms_id  (GdaDict *dict, const gchar *dbms_id);

/* GdaQuery manipulations */
void            gda_dict_declare_query       (GdaDict *dict, GdaQuery *query);
void            gda_dict_assume_query        (GdaDict *dict, GdaQuery *query);
void            gda_dict_unassume_query      (GdaDict *dict, GdaQuery *query);
GSList         *gda_dict_get_queries         (GdaDict *dict);
GdaQuery        *gda_dict_get_query_by_xml_id (GdaDict *dict, const gchar *xml_id);

/* GdaGraph manipulations */
void            gda_dict_declare_graph       (GdaDict *dict, GdaGraph *graph);
void            gda_dict_assume_graph        (GdaDict *dict, GdaGraph *graph);
void            gda_dict_unassume_graph      (GdaDict *dict, GdaGraph *graph);
GSList         *gda_dict_get_graphs          (GdaDict *dict, GdaGraphType type_of_graphs);
GdaGraph        *gda_dict_get_graph_by_xml_id (GdaDict *dict, const gchar *xml_id);
GdaGraph        *gda_dict_get_graph_for_object(GdaDict *dict, GObject *obj);

GdaDictDatabase  *gda_dict_get_database        (GdaDict *dict);

GSList         *gda_dict_get_entities_fk_constraints (GdaDict *dict, GdaEntity *entity1, GdaEntity *entity2,
 						     gboolean entity1_has_fk);

#ifdef GDA_DEBUG
void            gda_dict_dump                (GdaDict *dict);
#endif
G_END_DECLS

void            gda_connection_add_data_type (GdaDict *dict, GdaDictType *datatype);

#endif
