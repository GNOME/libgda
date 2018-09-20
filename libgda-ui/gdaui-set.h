/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-set.h>

G_BEGIN_DECLS

typedef struct _GdauiSetGroup GdauiSetGroup;
typedef struct _GdauiSetSource GdauiSetSource;

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


#define GDAUI_TYPE_SET          (gdaui_set_get_type())
G_DECLARE_DERIVABLE_TYPE (GdauiSet, gdaui_set, GDAUI, SET, GObject)

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
GdauiSet         *gdaui_set_new                 (GdaSet *set);
GdauiSetGroup    *gdaui_set_get_group           (GdauiSet *dbset, GdaHolder *holder);
GSList           *gdaui_set_get_sources         (GdauiSet *set);
GSList           *gdaui_set_get_groups          (GdauiSet *set);

G_END_DECLS

#endif



