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

#ifdef GSEAL_ENABLE
#else
struct _GdauiSetGroup {
        GdaSetGroup*      GSEAL(group);
        GdauiSetSource*   GSEAL(source); /* if NULL, then @group->nodes contains exactly one entry */

	/*< private >*/
        /* Padding for future expansion */
        gpointer      GSEAL(_gda_reserved1);
        gpointer      GSEAL(_gda_reserved2);
};
#endif

#define GDAUI_TYPE_SET_GROUP (gdaui_set_group_get_type ())
#define GDAUI_SET_GROUP(x) ((GdauiSetGroup*)(x))

GType             gdaui_set_group_get_type           (void) G_GNUC_CONST;
GdauiSetGroup    *gdaui_set_group_new                (GdaSetGroup *group);
void              gdaui_set_group_free               (GdauiSetGroup *sg);
GdauiSetGroup    *gdaui_set_group_copy               (GdauiSetGroup *sg);
void              gdaui_set_group_set_source         (GdauiSetGroup *sg, GdauiSetSource *source);
GdauiSetSource   *gdaui_set_group_get_source         (GdauiSetGroup *sg);
void              gdaui_set_group_set_group          (GdauiSetGroup *sg, GdaSetGroup *group);
GdaSetGroup      *gdaui_set_group_get_group          (GdauiSetGroup *sg);

#ifdef GSEAL_ENABLE
#else
struct _GdauiSetSource {
        GdaSetSource*   GSEAL(source);

	/* displayed columns in 'source->data_model' */
 	gint  GSEAL(shown_n_cols);
 	gint* GSEAL(shown_cols_index);

 	/* columns used as a reference (corresponding to PK values) in 'source->data_model' */
 	gint  GSEAL(ref_n_cols);
 	gint* GSEAL(ref_cols_index); 

	/*< private >*/
        /* Padding for future expansion */
        gpointer        GSEAL(_gda_reserved1);
        gpointer        GSEAL(_gda_reserved2);
        gpointer        GSEAL(_gda_reserved3);
        gpointer        GSEAL(_gda_reserved4);
};
#endif

#define GDAUI_TYPE_SET_SOURCE (gdaui_set_source_get_type ())
#define GDAUI_SET_SOURCE(x) ((GdauiSetSource*)(x))
GType             gdaui_set_source_get_type           (void) G_GNUC_CONST;
GdauiSetSource   *gdaui_set_source_new                (GdaSetSource *source);
void              gdaui_set_source_free               (GdauiSetSource *s);
GdauiSetSource   *gdaui_set_source_copy               (GdauiSetSource *s);
void              gdaui_set_source_set_source         (GdauiSetSource *s, GdaSetSource *source);
GdaSetSource     *gdaui_set_source_get_source         (GdauiSetSource*s);
gint              gdaui_set_source_get_shown_n_cols   (GdauiSetSource *s);
gint*             gdaui_set_source_get_shown_columns  (GdauiSetSource *s);
void              gdaui_set_source_set_shown_columns  (GdauiSetSource *s, gint *columns, gint n_columns);
gint              gdaui_set_source_get_ref_n_cols     (GdauiSetSource *s);
gint*             gdaui_set_source_get_ref_columns    (GdauiSetSource *s);
void              gdaui_set_source_set_ref_columns    (GdauiSetSource *s, gint *columns, gint n_columns);


/* struct for the object's data */
/* FIXME: public members of GdauiSet must be SEALED! */
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

/* Deprecated functions */
GType             _gdaui_set_get_type            (void);
GdauiSet         *_gdaui_set_new                 (GdaSet *set);
GdauiSetGroup    *_gdaui_set_get_group           (GdauiSet *dbset, GdaHolder *holder);
G_END_DECLS

#endif



