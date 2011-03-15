/* gda-threader.h
 *
 * Copyright (C) 2005 Vivien Malerba
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
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

#ifndef __GDA_THREADER_H_
#define __GDA_THREADER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_THREADER          (gda_threader_get_type())
#define GDA_THREADER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_threader_get_type(), GdaThreader)
#define GDA_THREADER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_threader_get_type (), GdaThreaderClass)
#define GDA_IS_THREADER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_threader_get_type ())


typedef struct _GdaThreader GdaThreader;
typedef struct _GdaThreaderClass GdaThreaderClass;
typedef struct _GdaThreaderPrivate GdaThreaderPrivate;
typedef void (*GdaThreaderFunc) (GdaThreader *, guint, gpointer);

/* struct for the object's data */
struct _GdaThreader
{
	GObject             object;
	GdaThreaderPrivate *priv;
};

/* struct for the object's class */
struct _GdaThreaderClass
{
	GObjectClass            parent_class;

	/* signals */
	void        (*finished)       (GdaThreader *thread, guint job_id, gpointer arg_data);
	void        (*cancelled)      (GdaThreader *thread, guint job_id, gpointer arg_data);
};

GType        gda_threader_get_type        (void) G_GNUC_CONST;
GObject     *gda_threader_new             (void);

guint        gda_threader_start_thread    (GdaThreader *thread, GThreadFunc func, gpointer func_arg, 
					   GdaThreaderFunc ok_callback, GdaThreaderFunc cancel_callback, 
					   GError **error);
void         gda_threader_cancel          (GdaThreader *thread, guint job_id);
G_END_DECLS

#endif
