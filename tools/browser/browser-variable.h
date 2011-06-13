/*
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __BROWSER_VARIABLE_H_
#define __BROWSER_VARIABLE_H_

#include <libgda/libgda.h>
#include "decl.h"

G_BEGIN_DECLS

#define BROWSER_TYPE_VARIABLE          (browser_variable_get_type())
#define BROWSER_VARIABLE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_variable_get_type(), BrowserVariable)
#define BROWSER_VARIABLE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_variable_get_type (), BrowserVariableClass)
#define BROWSER_IS_VARIABLE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_variable_get_type ())

typedef struct _BrowserVariableClass BrowserVariableClass;
typedef struct _BrowserVariablePrivate BrowserVariablePrivate;

/* struct for the object's data */
struct _BrowserVariable
{
	GObject                object;
	BrowserVariablePrivate *priv;
};

/* struct for the object's class */
struct _BrowserVariableClass
{
	GObjectClass            parent_class;
};

GType               browser_variable_get_type               (void) G_GNUC_CONST;
BrowserVariable    *browser_variable_new                    (GdaHolder *holder);

G_END_DECLS

#endif
