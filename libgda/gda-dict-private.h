/* gda-dict-private.c
 *
 * Copyright (C) 2006 Vivien Malerba
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

#ifndef __GDA_DICT_PRIVATE_H__
#define __GDA_DICT_PRIVATE_H__

#include "gda-dict.h"
#include "gda-connection.h"

struct _GdaDictPrivate
{
	GdaDictDatabase   *database;
	GdaConnection     *cnc;
	gchar             *xml_filename;

	gboolean           with_functions;

	gchar             *dsn;
	gchar             *user;

	/* DBMS update related information */
	gboolean           update_in_progress;
	gboolean           stop_update; /* TRUE if a DBMS data update must be stopped */

	/* DBMS objects */
	GSList            *users;

	/* Hash tables for fast retreival of any object */
	GHashTable        *object_ids; /* key = string ID, value = GdaObject for that ID */


	/* Generic interface for handled objects */
	GSList            *registry_list; /* list of GdaDictRegisterStruct */
	GHashTable        *registry; /* key = GType, value = GdaDictRegisterStruct ("referenced" here) */
	GHashTable        *registry_xml_groups; /* key = XML group tag, value = GdaDictRegisterStruct (not "referenced" here) */
	GHashTable        *objects_as_hash; /* key = object, value = GType as which it has been assumed */
};



/* registration mechanism */
typedef struct _GdaDictRegisterStruct GdaDictRegisterStruct;
typedef void     (*GdaDictRegFreeFunc) (GdaDict *, GdaDictRegisterStruct *);
typedef gboolean (*GdaDictRegSyncFunc) (GdaDict *, const gchar *limit_object_name, GError **);
typedef GSList  *(*GdaDictRegGetListFunc)  (GdaDict *);
typedef gboolean (*GdaDictRegSaveFunc) (GdaDict *, xmlNodePtr, GError **);
typedef gboolean (*GdaDictRegLoadFunc) (GdaDict *, xmlNodePtr, GError **);
typedef GdaObject *(*GdaDictGetObjFunc) (GdaDict *, const gchar *);

struct _GdaDictRegisterStruct {
	/* general information */
	GType                  type; /* type of object, always valid */
	gboolean               sort; /* TRUE if lists of objects are sorted by object name */
	GdaDictRegFreeFunc     free; /* to free @GdaDictRegisterStruct or NULL if just g_free() */

	/* lists of objects */
	GSList                *all_objects;
	GSList                *assumed_objects;

	/* operations on lists */
	gchar                 *dbms_sync_key;
	gchar                 *dbms_sync_descr;
	GdaDictRegSyncFunc     dbms_sync; /* DBMS sync, or NULL if no sync possible */
	GdaDictRegGetListFunc  get_objects; /* get the list of objects, or NULL if generic method */
	GdaDictGetObjFunc      get_by_name; /* get a specific object, or NULL if generic method */

	/* XML related */
	const gchar           *xml_group_tag; /* tag for the group of objects in XML file, or NULL if no storage */
	GdaDictRegSaveFunc     load_xml_tree; /* loading from XML file, or NULL if no loading */
	GdaDictRegLoadFunc     save_xml_tree; /* saving to XML file, or NULL if no saving*/
};

typedef                GdaDictRegisterStruct *(*GdaDictRegFunc) (void);
void                   gda_dict_class_always_register            (GdaDictRegFunc);
void                   gda_dict_register_object_type             (GdaDict *dict, GdaDictRegisterStruct *reg);
GdaDictRegisterStruct *gda_dict_get_object_type_registration     (GdaDict *dict, GType type);

#endif
