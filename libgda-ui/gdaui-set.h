/* gdaui-set.h
 *
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __GDAUI_SET__
#define __GDAUI_SET__

#include <gtk/gtk.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_SET          (_gdaui_set_get_type())
#define GDAUI_SET(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, _gdaui_set_get_type(), GdauiSet)
#define GDAUI_SET_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, _gdaui_set_get_type (), GdauiSetClass)
#define GDAUI_IS_SET(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, _gdaui_set_get_type ())


typedef struct _GdauiSet      GdauiSet;
typedef struct _GdauiSetClass GdauiSetClass;
typedef struct _GdauiSetPriv  GdauiSetPriv;

typedef struct _GdauiSetGroup GdauiSetGroup;
typedef struct _GdauiSetSource GdauiSetSource;

struct _GdauiSetGroup {
        GdaSetGroup      *group;
        GdauiSetSource   *source; /* if NULL, then @group->nodes contains exactly one entry */

	/*< private >*/
        /* Padding for future expansion */
        gpointer      _gda_reserved1;
        gpointer      _gda_reserved2;
};

#define GDAUI_SET_GROUP(x) ((GdauiSetGroup*)(x))

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

#define GDAUI_SET_SOURCE(x) ((GdauiSetSource*)(x))

/* struct for the object's data */
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
};

/* 
 * Generic widget's methods 
 */
GType             _gdaui_set_get_type            (void) G_GNUC_CONST;

GdauiSet         *_gdaui_set_new                 (GdaSet *set);
GdauiSetGroup    *_gdaui_set_get_group          (GdauiSet *dbset, GdaHolder *holder);

G_END_DECLS

#endif



