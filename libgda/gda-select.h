/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_select_h__)
#  define __gda_select_h__

#include <libgda/gda-data-model-array.h>

G_BEGIN_DECLS

#define GDA_TYPE_SELECT            (gda_select_get_type())
#define GDA_SELECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SELECT, GdaSelect))
#define GDA_SELECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SELECT, GdaSelectClass))
#define GDA_IS_SELECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_SELECT))
#define GDA_IS_SELECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_SELECT))

typedef struct _GdaSelect        GdaSelect;
typedef struct _GdaSelectClass   GdaSelectClass;
typedef struct _GdaSelectPrivate GdaSelectPrivate;

struct _GdaSelect {
	GdaDataModelArray model;
	GdaSelectPrivate *priv;
};

struct _GdaSelectClass {
	GdaDataModelArrayClass parent_class;
};

GType         gda_select_get_type (void);
GdaDataModel *gda_select_new (void);
void          gda_select_set_source (GdaSelect *sel, GdaDataModel *source);
void          gda_select_set_expression (GdaSelect *sel, const gchar *expression);
gboolean      gda_select_run (GdaSelect *sel);

G_END_DECLS

#endif
