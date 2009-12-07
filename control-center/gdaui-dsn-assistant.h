/* GNOME DB library
 * Copyright (C) 1999 - 2008 The GNOME Foundation
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
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

#ifndef __GDAUI_DSN_ASSISTANT_H__
#define __GDAUI_DSN_ASSISTANT_H__

#include <libgda/gda-config.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DSN_ASSISTANT            (gdaui_dsn_assistant_get_type())
#define GDAUI_DSN_ASSISTANT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_DSN_ASSISTANT, GdauiDsnAssistant))
#define GDAUI_DSN_ASSISTANT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDAUI_TYPE_DSN_ASSISTANT, GdauiDsnAssistantClass))
#define GDAUI_IS_DSN_ASSISTANT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_DSN_ASSISTANT))
#define GDAUI_IS_DSN_ASSISTANT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_DSN_ASSISTANT))

typedef struct _GdauiDsnAssistant        GdauiDsnAssistant;
typedef struct _GdauiDsnAssistantClass   GdauiDsnAssistantClass;
typedef struct _GdauiDsnAssistantPrivate GdauiDsnAssistantPrivate;

struct _GdauiDsnAssistant {
	GtkAssistant                assistant;
	GdauiDsnAssistantPrivate *priv;
};

struct _GdauiDsnAssistantClass {
	GtkAssistantClass           parent_class;

	void                     (* finished) (GdauiDsnAssistant *assistant, gboolean error);
};

GType                    gdaui_dsn_assistant_get_type (void) G_GNUC_CONST;

GtkWidget               *gdaui_dsn_assistant_new      (void);
const GdaDsnInfo *gdaui_dsn_assistant_get_dsn  (GdauiDsnAssistant *assistant);

G_END_DECLS

#endif
