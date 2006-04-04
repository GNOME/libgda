/* GDA common library
 * Copyright (C) 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DATA_ACCESS_WRAPPER_H__
#define __GDA_DATA_ACCESS_WRAPPER_H__

#include <libgda/gda-object.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_ACCESS_WRAPPER            (gda_data_access_wrapper_get_type())
#define GDA_DATA_ACCESS_WRAPPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_ACCESS_WRAPPER, GdaDataAccessWrapper))
#define GDA_DATA_ACCESS_WRAPPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_ACCESS_WRAPPER, GdaDataAccessWrapperClass))
#define GDA_IS_DATA_ACCESS_WRAPPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_DATA_ACCESS_WRAPPER))
#define GDA_IS_DATA_ACCESS_WRAPPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_DATA_ACCESS_WRAPPER))

typedef struct _GdaDataAccessWrapper        GdaDataAccessWrapper;
typedef struct _GdaDataAccessWrapperClass   GdaDataAccessWrapperClass;
typedef struct _GdaDataAccessWrapperPrivate GdaDataAccessWrapperPrivate;

struct _GdaDataAccessWrapper {
	GdaObject                      object;
	GdaDataAccessWrapperPrivate *priv;
};

struct _GdaDataAccessWrapperClass {
	GdaObjectClass                 parent_class;
};

GType         gda_data_access_wrapper_get_type    (void);
GdaDataModel *gda_data_access_wrapper_new         (GdaDataModel *model);
gboolean      gda_data_access_wrapper_row_exists  (GdaDataAccessWrapper *wrapper, gint row);

G_END_DECLS

#endif
