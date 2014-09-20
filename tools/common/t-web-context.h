/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __T_WEB_CONTEXT__
#define __T_WEB_CONTEXT__

#include <glib.h>
#include "t-context.h"

/*
 * Object representing a context using a connection (AKA a console)
 */
#define T_TYPE_WEB_CONTEXT          (t_web_context_get_type())
#define T_WEB_CONTEXT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, t_web_context_get_type(), TWebContext)
#define T_WEB_CONTEXT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, t_web_context_get_type (), TWebContextClass)
#define T_IS_WEB_CONTEXT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, t_web_context_get_type ())

typedef struct _TWebContextClass TWebContextClass;
typedef struct _TWebContextPrivate TWebContextPrivate;

/* struct for the object's data */
typedef struct
{
        TContext            object;
        TWebContextPrivate *priv;
} TWebContext;

/* struct for the object's class */
struct _TWebContextClass
{
        TContextClass       parent_class;
};

GType             t_web_context_get_type (void);
TWebContext      *t_web_context_new (const gchar *id);

#endif
