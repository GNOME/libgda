/* gda-dict.h
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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

#ifndef __GDA_DICT_H_
#define __GDA_DICT_H_

#include <glib-object.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-value.h>
#include <libgda/gda-enums.h>

/* 
You will need to include these when including gda-dict.h, 
to use some functions.
TODO: Avoid this awkwarness:
#include <libgda/gda-dict-reg-aggregates.h>
#include <libgda/gda-dict-reg-functions.h>
*/


G_BEGIN_DECLS

#define GDA_TYPE_DICT          (gda_dict_get_type())
#define GDA_DICT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_dict_get_type(), GdaDict)
#define GDA_DICT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_dict_get_type (), GdaDictClass)
#define GDA_IS_DICT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_dict_get_type ())


/* error reporting */
extern GQuark gda_dict_error_quark (void);
#define GDA_DICT_ERROR gda_dict_error_quark ()

typedef enum {
	GDA_DICT_META_DATA_UPDATE_ERROR,
	GDA_DICT_META_DATA_UPDATE_USER_STOPPED,
	GDA_DICT_LOAD_FILE_NOT_EXIST_ERROR,
	GDA_DICT_FILE_LOAD_ERROR,
	GDA_DICT_FILE_SAVE_ERROR,
	GDA_DICT_DATATYPE_ERROR,
	GDA_DICT_FUNCTIONS_ERROR,
	GDA_DICT_AGGREGATES_ERROR
} GdaDictError;


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

	/* signals the addition, removal or update of objects */
	void    (*object_added)         (GdaDict *dict, GdaObject *obj);
	void    (*object_removed)       (GdaDict *dict, GdaObject *obj);
	void    (*object_updated)       (GdaDict *dict, GdaObject *obj);
	void    (*object_act_changed)   (GdaDict *dict, GdaObject *obj);

        void    (*data_update_started)  (GdaDict *dict);
        void    (*update_progress)      (GdaDict *dict, gchar *msg, guint now, guint total);
        void    (*data_update_finished) (GdaDict *dict);

	/* signal that a change in the whole dictionary has occurred */
	void    (*changed)              (GdaDict * dict);

	/* class variable */
	GSList   *class_registry_list; /* list of GdaDictRegFunc functions */
	gpointer  reserved1;
	gpointer  reserved2;
};

GType             gda_dict_get_type                        (void) G_GNUC_CONST;
GdaDict          *gda_dict_new                             (void);
void              gda_dict_extend_with_functions           (GdaDict *dict);

void              gda_dict_set_connection                  (GdaDict *dict, GdaConnection *cnc);
GdaConnection    *gda_dict_get_connection                  (GdaDict *dict);
GdaDictDatabase  *gda_dict_get_database                    (GdaDict *dict);

void              gda_dict_declare_object_string_id_change (GdaDict *dict, GdaObject *obj, const gchar *oldid);
GdaObject        *gda_dict_get_object_by_string_id         (GdaDict *dict, const gchar *strid);

gboolean          gda_dict_update_dbms_meta_data           (GdaDict *dict, GType limit_to_type, const gchar *limit_obj_name, 
							    GError **error);
void              gda_dict_stop_update_dbms_meta_data      (GdaDict *dict);

gchar            *gda_dict_compute_xml_filename            (GdaDict *dict, const gchar *datasource, 
							    const gchar *app_id, GError **error);
void              gda_dict_set_xml_filename                (GdaDict *dict, const gchar *xmlfile);
const gchar      *gda_dict_get_xml_filename                (GdaDict *dict);
gboolean          gda_dict_load                            (GdaDict *dict, GError **error);
gboolean          gda_dict_save                            (GdaDict *dict, GError **error);
gboolean          gda_dict_load_xml_file                   (GdaDict *dict, const gchar *xmlfile, GError **error);
gboolean          gda_dict_save_xml_file                   (GdaDict *dict, const gchar *xmlfile, GError **error);

GdaDataHandler   *gda_dict_get_handler                     (GdaDict *dict, GType for_type);
GdaDataHandler   *gda_dict_get_default_handler             (GdaDict *dict, GType for_type);

/* GdaQuery manipulations */
#define gda_dict_get_queries(dict) gda_dict_get_objects((dict), GDA_TYPE_QUERY)
#define gda_dict_get_query_by_xml_id(dict,xml_id) ((GdaQuery*)gda_dict_get_object_by_xml_id((dict), GDA_TYPE_QUERY, (xml_id)))

/* GdaDictType manipulations */
#define gda_dict_get_dict_types(dict) gda_dict_get_objects((dict), GDA_TYPE_DICT_TYPE)
#define gda_dict_get_dict_type_by_name(dict,type_name) ((GdaDictType*)gda_dict_get_object_by_name((dict), GDA_TYPE_DICT_TYPE, (type_name)))
#define gda_dict_get_dict_type_by_xml_id(dict,xml_id) ((GdaDictType*)gda_dict_get_object_by_xml_id((dict), GDA_TYPE_DICT_TYPE, (xml_id)))

/* GdaDictFunction manipulations */
#define gda_dict_get_functions(dict) gda_dict_get_objects ((dict), GDA_TYPE_DICT_FUNCTION)
#define gda_dict_get_functions_by_name(dict,funcname) gda_functions_get_by_name ((dict), (funcname))
#define gda_dict_get_function_by_name_arg(dict,funcname,argtypes) gda_functions_get_by_name_arg ((dict), (funcname), (argtypes))
#define gda_dict_get_function_by_xml_id(dict,xml_id) gda_dict_get_object_by_xml_id ((dict), GDA_TYPE_DICT_FUNCTION, (xml_id))
#define gda_dict_get_function_by_dbms_id(dict,dbms_id) gda_functions_get_by_dbms_id ((dict), (dbms_id))

/* GdaDictAggregate manipulations */
#define gda_dict_get_aggregates(dict) gda_dict_get_objects((dict), GDA_TYPE_DICT_AGGREGATE)
#define gda_dict_get_aggregates_by_name(dict,aggname) gda_aggregates_get_by_name((dict), (aggname))
#define gda_dict_get_aggregate_by_name_arg(dict,argname,argtype) gda_aggregates_get_by_name_arg ((dict), (argname), (argtype))
#define gda_dict_get_aggregate_by_xml_id(dict,xml_id) gda_dict_get_object_by_xml_id((dict), GDA_TYPE_DICT_AGGREGATE, (xml_id))
#define gda_dict_get_aggregate_by_dbms_id(dict,dbmsid) gda_aggregates_get_by_dbms_id((dict),(dbmsid))


void              gda_dict_declare_object                  (GdaDict *dict, GdaObject *object);
void              gda_dict_declare_object_as               (GdaDict *dict, GdaObject *object, GType as_type);
void              gda_dict_assume_object                   (GdaDict *dict, GdaObject *object);
void              gda_dict_assume_object_as                (GdaDict *dict, GdaObject *object, GType as_type);
void              gda_dict_unassume_object                 (GdaDict *dict, GdaObject *object);
gboolean          gda_dict_object_is_assumed               (GdaDict *dict, GdaObject *object);
GSList           *gda_dict_get_objects                     (GdaDict *dict, GType type);
GdaObject        *gda_dict_get_object_by_name              (GdaDict *dict, GType type, const gchar *name);
GdaObject        *gda_dict_get_object_by_xml_id            (GdaDict *dict, GType type, const gchar *xml_id);

#ifdef GDA_DEBUG
void              gda_dict_dump                            (GdaDict *dict);
#endif
G_END_DECLS

#endif
