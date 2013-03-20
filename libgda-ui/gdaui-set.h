/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2013 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDAUI_SET__
#define __GDAUI_SET__

#include <gtk/gtk.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_SET          (gdaui_set_get_type())
#define GDAUI_SET(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_set_get_type(), GdauiSet)
#define GDAUI_SET_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_set_get_type (), GdauiSetClass)
#define GDAUI_IS_SET(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_set_get_type ())


typedef struct _GdauiSet      GdauiSet;
typedef struct _GdauiSetClass GdauiSetClass;
typedef struct _GdauiSetPriv  GdauiSetPriv;

typedef struct _GdauiSetGroup GdauiSetGroup;
typedef struct _GdauiSetSource GdauiSetSource;

/**
 * GdauiSetGroup:
 * @group: 
 * @source: 
 *
 * The <structname>GdauiSetGroup</structname>.
 *
 * To create a new #GdauiSetGroup use #gdaiu_set_group_new. 
 * 
 * To free a #GdauiSetGroup, created by #gdaui_set_group_new, use #gda_set_group_free.
 *
 * Since 5.2, you must consider this struct as opaque. Any access to its internal must use public API.
 * Don't try to use #gdaui_set_group_free on a struct that was created manually.
 */
struct _GdauiSetGroup {
        GdaSetGroup      *group;
        GdauiSetSource   *source; /* if NULL, then @group->nodes contains exactly one entry */

	/*< private >*/
        /* Padding for future expansion */
        gpointer      _gda_reserved1;
        gpointer      _gda_reserved2;
};

/**
 * GdauiSetSource:
 * @source: a #GdaSetSource
 * @shown_cols_index: (array length=shown_n_cols): Array with the column number to be shown from #GdaSetSource
 * @shown_n_cols: number of elements of @shown_cols_index
 * @ref_cols_index: (array length=ref_n_cols): Array with the number of columns as PRIMARY KEY in #GdaSetSource
 * @ref_n_cols: number of elements of @ref_cols_index
 *
 * The <structname>GdauiSetSource</structname> is a ...
 *
 * To create a new #GdauiSetSource use #gdaui_set_source_new.
 * 
 * To free a #GdauiSetSource, created by #gdaui_set_source_new, use #gdaui_set_source_free.
 *
 * Since 5.2, you must consider this struct as opaque. Any access to its internal must use public API.
 * Don't try to use #gdaui_set_source_free on a struct that was created manually.
 */
struct _GdauiSetSource {
        GdaSetSource   *source;

	/* displayed columns in 'source->data_model' */
 	gint shown_n_cols;
 	gint *shown_cols_index;

 	/* columns used as a reference (corresponding to PK values) in 'source->data_model' */
 	gint ref_n_cols;
 	gint *ref_cols_index; 

	/*< private >*/
        /* Padding for future expansion */
        gpointer        _gda_reserved1;
        gpointer        _gda_reserved2;
        gpointer        _gda_reserved3;
        gpointer        _gda_reserved4;
};



/* struct for the object's data */
/**
 * GdauiSet:
 * @sources_list: (element-type Gdaui.SetSource): list of #GdauiSetSource
 * @groups_list: (element-type Gdaui.SetGroup): list of #GdauiSetGroup
 */
struct _GdauiSet
{
	GObject         object;
	GdauiSetPriv   *priv;

	/*< public >*/
	GSList         *sources_list; /* list of GdauiSetSource */
        GSList         *groups_list;  /* list of GdauiSetGroup */
};

/* struct for the object's class */
struct _GdauiSetClass
{
	GObjectClass       parent_class;
	void             (*public_data_changed)   (GdauiSet *set);
	void             (*source_model_changed)  (GdauiSet *set, GdauiSetSource *source);
};

/* 
 * Generic widget's methods 
 */
GType             gdaui_set_get_type            (void) G_GNUC_CONST;
GdauiSet         *gdaui_set_new                 (GdaSet *set);
GdauiSetGroup    *gdaui_set_get_group           (GdauiSet *dbset, GdaHolder *holder);

#define GDAUI_TYPE_SET_GROUP (gdaui_set_group_get_type ())
#define GDAUI_SET_GROUP(x) ((GdauiSetGroup*)(x))
GType             gdaui_set_group_get_type           (void) G_GNUC_CONST;
GdauiSetGroup    *gdaui_set_group_new                (void);
void              gdaui_set_group_free               (GdauiSetGroup *sg);
GdauiSetGroup    *gdaui_set_group_copy               (GdauiSetGroup *sg);
void              gdaui_set_group_set_source         (GdauiSetGroup *sg, GdauiSetSource *source);
GdauiSetSource   *gdaui_set_group_get_source         (GdauiSetGroup *sg);
void              gdaui_set_group_set_group          (GdauiSetGroup *sg, GdaSetGroup *group);
GdaSetGroup      *gdaui_set_group_get_group          (GdauiSetGroup *sg);

#define GDAUI_TYPE_SET_SOURCE (gdaui_set_source_get_type ())
#define GDAUI_SET_SOURCE(x) ((GdauiSetSource*)(x))
GType             gdaui_set_source_get_type           (void) G_GNUC_CONST;
GdauiSetSource   *gdaui_set_source_new                (void);
void              gdaui_set_source_free               (GdauiSetSource *s);
GdauiSetSource   *gdaui_set_source_copy               (GdauiSetSource *s);
void              gdaui_set_source_set_source         (GdauiSetSource *s, GdaSetSource *source);
GdaSetSource     *gdaui_set_source_get_source         (GdauiSetSource*s);
void              gdaui_set_source_set_shown_columns  (GdauiSetSource *s, gint *columns, gint n_columns);
void              gdaui_set_source_set_ref_columns    (GdauiSetSource *s, gint *columns, gint n_columns);

/* Deprecated functions */
GType             _gdaui_set_get_type            (void);
GdauiSet         *_gdaui_set_new                 (GdaSet *set);
GdauiSetGroup    *_gdaui_set_get_group           (GdauiSet *dbset, GdaHolder *holder);
G_END_DECLS

#endif



